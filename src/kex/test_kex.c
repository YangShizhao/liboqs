#if defined(WINDOWS)
#pragma warning(disable : 4244 4293)
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <oqs/kex.h>
#include <oqs/rand.h>

#include "../ds_benchmark.h"
#include "../common/common.h"

struct kex_testcase {
	enum OQS_KEX_alg_name alg_name;
	unsigned char *seed;
	size_t seed_len;
	char *named_parameters;
	char *id;
	int run;
	int iter;
};

/* Add new testcases here */
struct kex_testcase kex_testcases[] = {
#ifdef ENABLE_KEX_LWE_FRODO
    {OQS_KEX_alg_lwe_frodo, (unsigned char *) "01234567890123456", 16, "recommended", "lwe_frodo_recommended", 0, 100},
#endif
#ifdef ENABLE_CODE_MCBITS
    {OQS_KEX_alg_code_mcbits, NULL, 0, NULL, "code_mcbits", 0, 25},
#endif
#ifndef DISABLE_NTRU_ON_WINDOWS_BY_DEFAULT
#ifdef ENABLE_KEX_NTRU
    {OQS_KEX_alg_ntru, NULL, 0, NULL, "ntru", 0, 25},
#endif
#endif
    {OQS_KEX_alg_rlwe_bcns15, NULL, 0, NULL, "rlwe_bcns15", 0, 100},
#ifdef ENABLE_KEX_RLWE_MSRLN16
    {OQS_KEX_alg_rlwe_msrln16, NULL, 0, NULL, "rlwe_msrln16", 0, 100},
#endif
#ifdef ENABLE_KEX_RLWE_NEWHOPE
    {OQS_KEX_alg_rlwe_newhope, NULL, 0, NULL, "rlwe_newhope", 0, 100},
#endif
#ifdef ENABLE_KEX_SIDH_CLN16
    {OQS_KEX_alg_sidh_cln16, NULL, 0, NULL, "sidh_cln16", 0, 10},
    {OQS_KEX_alg_sidh_cln16_compressed, NULL, 0, NULL, "sidh_cln16_compressed", 0, 10},
#endif
#ifdef ENABLE_SIDH_IQC_REF
    {OQS_KEX_alg_sidh_iqc_ref, NULL, 0, "params771", "sidh_iqc_ref", 0, 10},
#endif
#ifdef ENABLE_KEX_RLWE_NEWHOPE_AVX2
    {OQS_KEX_alg_rlwe_newhope_avx2, NULL, 0, NULL, "rlwe_newhope_avx2", 0, 100},
#endif

};

#define KEX_TEST_ITERATIONS 100
#define KEX_BENCH_SECONDS_DEFAULT 1

#define PRINT_HEX_STRING(label, str, len)                        \
	{                                                            \
		printf("%-20s (%4zu bytes):  ", (label), (size_t)(len)); \
		for (size_t i = 0; i < (len); i++) {                     \
			printf("%02X", ((unsigned char *) (str))[i]);        \
		}                                                        \
		printf("\n");                                            \
	}

static int kex_test_correctness(OQS_RAND *rand, enum OQS_KEX_alg_name alg_name, const uint8_t *seed, const size_t seed_len, const char *named_parameters, const int print, unsigned long occurrences[256]) {

	OQS_KEX *kex = NULL;
	int rc;

	void *alice_priv = NULL;
	uint8_t *alice_msg = NULL;
	size_t alice_msg_len;
	uint8_t *alice_key = NULL;
	size_t alice_key_len;

	uint8_t *bob_msg = NULL;
	size_t bob_msg_len;
	uint8_t *bob_key = NULL;
	size_t bob_key_len;

	/* setup KEX */
	kex = OQS_KEX_new(rand, alg_name, seed, seed_len, named_parameters);
	if (kex == NULL) {
		eprintf("new_method failed\n");
		goto err;
	}

	if (print) {
		printf("================================================================================\n");
		printf("Sample computation for key exchange method %s\n", kex->method_name);
		printf("================================================================================\n");
	}

	/* Alice's initial message */
	rc = OQS_KEX_alice_0(kex, &alice_priv, &alice_msg, &alice_msg_len);
	if (rc != 1) {
		eprintf("OQS_KEX_alice_0 failed\n");
		goto err;
	}

	if (print) {
		PRINT_HEX_STRING("Alice message", alice_msg, alice_msg_len)
	}

	/* Bob's response */
	rc = OQS_KEX_bob(kex, alice_msg, alice_msg_len, &bob_msg, &bob_msg_len, &bob_key, &bob_key_len);
	if (rc != 1) {
		eprintf("OQS_KEX_bob failed\n");
		goto err;
	}

	if (print) {
		PRINT_HEX_STRING("Bob message", bob_msg, bob_msg_len)
		PRINT_HEX_STRING("Bob session key", bob_key, bob_key_len)
	}

	/* Alice processes Bob's response */
	rc = OQS_KEX_alice_1(kex, alice_priv, bob_msg, bob_msg_len, &alice_key, &alice_key_len);
	if (rc != 1) {
		eprintf("OQS_KEX_alice_1 failed\n");
		goto err;
	}

	if (print) {
		PRINT_HEX_STRING("Alice session key", alice_key, alice_key_len)
	}

	/* compare session key lengths and values */
	if (alice_key_len != bob_key_len) {
		eprintf("ERROR: Alice's session key and Bob's session key are different lengths (%zu vs %zu)\n", alice_key_len, bob_key_len);
		goto err;
	}
	rc = memcmp(alice_key, bob_key, alice_key_len);
	if (rc != 0) {
		eprintf("ERROR: Alice's session key and Bob's session key are not equal\n");
		PRINT_HEX_STRING("Alice session key", alice_key, alice_key_len)
		PRINT_HEX_STRING("Bob session key", bob_key, bob_key_len)
		goto err;
	}
	if (print) {
		printf("Alice and Bob's session keys match.\n");
		printf("\n\n");
	}

	/* record generated bytes for statistical analysis */
	for (size_t i = 0; i < alice_key_len; i++) {
		OQS_RAND_test_record_occurrence(alice_key[i], occurrences);
	}

	rc = 1;
	goto cleanup;

err:
	rc = 0;

cleanup:
	free(alice_msg);
	free(alice_key);
	free(bob_msg);
	free(bob_key);
	OQS_KEX_alice_priv_free(kex, alice_priv);
	OQS_KEX_free(kex);

	return rc;
}

static int kex_test_correctness_wrapper(OQS_RAND *rand, enum OQS_KEX_alg_name alg_name, const uint8_t *seed, const size_t seed_len, const char *named_parameters, int iterations, bool quiet) {
	OQS_KEX *kex = NULL;
	int ret;

	unsigned long occurrences[256];
	for (int i = 0; i < 256; i++) {
		occurrences[i] = 0;
	}

	ret = kex_test_correctness(rand, alg_name, seed, seed_len, named_parameters, quiet ? 0 : 1, occurrences);

	if (ret != 1) {
		goto err;
	}

	/* setup KEX */
	kex = OQS_KEX_new(rand, alg_name, seed, seed_len, named_parameters);
	if (kex == NULL) {
		goto err;
	}

	printf("================================================================================\n");
	printf("Testing correctness and randomness of key exchange method %s (params=%s) for %d iterations\n",
	       kex->method_name, named_parameters, iterations);
	printf("================================================================================\n");
	for (int i = 0; i < iterations; i++) {
		ret = kex_test_correctness(rand, alg_name, seed, seed_len, named_parameters, 0, occurrences);
		if (ret != 1) {
			goto err;
		}
	}
	printf("All session keys matched.\n");
	OQS_RAND_report_statistics(occurrences, "");
	printf("\n\n");

	ret = 1;
	goto cleanup;

err:
	ret = 0;

cleanup:
	OQS_KEX_free(kex);

	return ret;
}

static void cleanup_alice_0(OQS_KEX *kex, void *alice_priv, uint8_t *alice_msg) {
	free(alice_msg);
	OQS_KEX_alice_priv_free(kex, alice_priv);
}

static void cleanup_bob(uint8_t *bob_msg, uint8_t *bob_key) {
	free(bob_msg);
	free(bob_key);
}

static int kex_bench_wrapper(OQS_RAND *rand, enum OQS_KEX_alg_name alg_name, const uint8_t *seed, const size_t seed_len, const char *named_parameters, const size_t seconds) {

	OQS_KEX *kex = NULL;
	int rc;

	void *alice_priv = NULL;
	uint8_t *alice_msg = NULL;
	size_t alice_msg_len;
	uint8_t *alice_key = NULL;
	size_t alice_key_len;

	uint8_t *bob_msg = NULL;
	size_t bob_msg_len;
	uint8_t *bob_key = NULL;
	size_t bob_key_len;

	/* setup KEX */
	kex = OQS_KEX_new(rand, alg_name, seed, seed_len, named_parameters);
	if (kex == NULL) {
		eprintf("new_method failed\n");
		goto err;
	}
	printf("%-30s | %10s | %14s | %15s | %10s | %16s | %10s\n", kex->method_name, "", "", "", "", "", "");

	TIME_OPERATION_SECONDS({ OQS_KEX_alice_0(kex, &alice_priv, &alice_msg, &alice_msg_len); cleanup_alice_0(kex, alice_priv, alice_msg); }, "alice 0", seconds);

	OQS_KEX_alice_0(kex, &alice_priv, &alice_msg, &alice_msg_len);
	TIME_OPERATION_SECONDS({ OQS_KEX_bob(kex, alice_msg, alice_msg_len, &bob_msg, &bob_msg_len, &bob_key, &bob_key_len); cleanup_bob(bob_msg, bob_key); }, "bob", seconds);

	OQS_KEX_bob(kex, alice_msg, alice_msg_len, &bob_msg, &bob_msg_len, &bob_key, &bob_key_len);
	TIME_OPERATION_SECONDS({ OQS_KEX_alice_1(kex, alice_priv, bob_msg, bob_msg_len, &alice_key, &alice_key_len); free(alice_key); }, "alice 1", seconds);
	alice_key = NULL;

	printf("Communication (bytes): A->B: %zu, B->A: %zu, total: %zu; classical/quantum security bits [%u:%u] \n", alice_msg_len, bob_msg_len, alice_msg_len + bob_msg_len, kex->estimated_classical_security, kex->estimated_quantum_security);

	rc = 1;
	goto cleanup;

err:
	rc = 0;

cleanup:
	free(alice_msg);
	free(alice_key);
	free(bob_msg);
	free(bob_key);
	OQS_KEX_alice_priv_free(kex, alice_priv);
	OQS_KEX_free(kex);

	return rc;
}

static int kex_mem_bench_wrapper(OQS_RAND *rand, enum OQS_KEX_alg_name alg_name, const uint8_t *seed, const size_t seed_len, const char *named_parameters) {

	OQS_KEX *kex = NULL;
	int rc;

	void *alice_priv = NULL;
	uint8_t *alice_msg = NULL;
	size_t alice_msg_len;
	uint8_t *alice_key = NULL;
	size_t alice_key_len;

	uint8_t *bob_msg = NULL;
	size_t bob_msg_len;
	uint8_t *bob_key = NULL;
	size_t bob_key_len;

	kex = OQS_KEX_new(rand, alg_name, seed, seed_len, named_parameters);
	if (kex == NULL) {
		fprintf(stderr, "new_method failed\n");
		goto err;
	}

	printf("running %s..\n", kex->method_name);

	OQS_KEX_alice_0(kex, &alice_priv, &alice_msg, &alice_msg_len);
	OQS_KEX_bob(kex, alice_msg, alice_msg_len, &bob_msg, &bob_msg_len, &bob_key, &bob_key_len);
	OQS_KEX_alice_1(kex, alice_priv, bob_msg, bob_msg_len, &alice_key, &alice_key_len);

	rc = 1;
	goto cleanup;

err:
	rc = 0;

cleanup:
	free(alice_msg);
	free(alice_key);
	free(bob_msg);
	free(bob_key);
	OQS_KEX_alice_priv_free(kex, alice_priv);
	OQS_KEX_free(kex);

	return rc;
}

void print_help() {
	printf("Usage: ./test_kex [options] [algorithms]\n");
	printf("\nOptions:\n");
	printf("  --quiet, -q\n");
	printf("    Less verbose output\n");
	printf("  --bench, -b\n");
	printf("    Run benchmarks\n");
	printf("  --seconds -s [SECONDS]\n");
	printf("    Number of seconds to run benchmarks (default==%d)\n", KEX_BENCH_SECONDS_DEFAULT);
	printf("  --mem-bench\n");
	printf("    Run memory benchmarks (run once and allocate only what is required)\n");
	printf("\nalgorithms:\n");
	size_t kex_testcases_len = sizeof(kex_testcases) / sizeof(struct kex_testcase);
	for (size_t i = 0; i < kex_testcases_len; i++) {
		printf("  %s\n", kex_testcases[i].id);
	}
}

int main(int argc, char **argv) {

	int success = 1;
	bool run_all = true;
	bool quiet = false;
	bool bench = false;
	bool mem_bench = false;
	size_t kex_testcases_len = sizeof(kex_testcases) / sizeof(struct kex_testcase);
	size_t kex_bench_seconds = KEX_BENCH_SECONDS_DEFAULT;
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			if ((strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "-help") == 0) || (strcmp(argv[i], "--help") == 0)) {
				print_help();
				return EXIT_SUCCESS;
			} else if (strcmp(argv[i], "--quiet") == 0 || strcmp(argv[i], "-q") == 0) {
				quiet = true;
			} else if (strcmp(argv[i], "--bench") == 0 || strcmp(argv[i], "-b") == 0) {
				bench = true;
			} else if (strcmp(argv[i], "--seconds") == 0 || strcmp(argv[i], "-s") == 0) {
				if (++i == argc) {
					print_help();
					return EXIT_SUCCESS;
				}
				char *end;
				int kex_bench_seconds_input = strtol(argv[i], &end, 10);
				if (kex_bench_seconds_input < 1) {
					print_help();
					return EXIT_SUCCESS;
				}
				kex_bench_seconds = kex_bench_seconds_input;
			} else if ((strcmp(argv[i], "--mem-bench") == 0 || strcmp(argv[i], "-m") == 0)) {
				mem_bench = true;
			}
		} else {
			run_all = false;
			for (size_t j = 0; j < kex_testcases_len; j++) {
				if (strcmp(argv[i], kex_testcases[j].id) == 0) {
					kex_testcases[j].run = 1;
				}
			}
		}
	}

	/* setup RAND */
	OQS_RAND *rand = OQS_RAND_new(OQS_RAND_alg_urandom_chacha20);
	if (rand == NULL) {
		goto err;
	}

	if (mem_bench) {
		for (size_t i = 0; i < kex_testcases_len; i++) {
			if (run_all || kex_testcases[i].run == 1) {
				success = kex_mem_bench_wrapper(rand, kex_testcases[i].alg_name, kex_testcases[i].seed, kex_testcases[i].seed_len, kex_testcases[i].named_parameters);
			}
			if (success != 1) {
				goto err;
			}
		}
		printf("memory benchmarks done, exiting..\n");
		success = 1;
		goto cleanup;
	}

	for (size_t i = 0; i < kex_testcases_len; i++) {
		if (run_all || kex_testcases[i].run == 1) {
			int num_iter = kex_testcases[i].iter;
			success = kex_test_correctness_wrapper(rand, kex_testcases[i].alg_name, kex_testcases[i].seed, kex_testcases[i].seed_len, kex_testcases[i].named_parameters, num_iter, quiet);
		}
		if (success != 1) {
			goto err;
		}
	}

	if (bench) {
		PRINT_TIMER_HEADER
		for (size_t i = 0; i < kex_testcases_len; i++) {
			if (run_all || kex_testcases[i].run == 1) {
				kex_bench_wrapper(rand, kex_testcases[i].alg_name, kex_testcases[i].seed, kex_testcases[i].seed_len, kex_testcases[i].named_parameters, kex_bench_seconds);
			}
		}
		PRINT_TIMER_FOOTER
	}

	success = 1;
	goto cleanup;

err:
	success = 0;
	eprintf("ERROR!\n");

cleanup:
	OQS_RAND_free(rand);

	return (success == 1) ? EXIT_SUCCESS : EXIT_FAILURE;
}
