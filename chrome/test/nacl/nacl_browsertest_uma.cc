// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/test/nacl/nacl_browsertest_util.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/nacl/browser/nacl_browser.h"
#include "components/nacl/renderer/platform_info.h"
#include "components/nacl/renderer/ppb_nacl_private.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "native_client/src/trusted/service_runtime/nacl_error_code.h"

namespace {

void CheckPNaClLoadUMAs(base::HistogramTester& histograms,
                        const std::string& compiler_suffix) {
  // The histograms that do not vary by compiler (so no compiler_suffix).
  histograms.ExpectTotalCount("NaCl.Perf.PNaClLoadTime.LoadLinker", 1);
  histograms.ExpectTotalCount("NaCl.Perf.PNaClLoadTime.LinkTime", 1);
  histograms.ExpectTotalCount("NaCl.Perf.Size.Manifest", 1);
  histograms.ExpectTotalCount("NaCl.Perf.Size.Pexe", 1);
  // The histograms that vary by compiler.
  histograms.ExpectTotalCount("NaCl.Options.PNaCl.OptLevel" + compiler_suffix,
                              1);
  histograms.ExpectTotalCount(
      "NaCl.Perf.Size.PNaClTranslatedNexe" + compiler_suffix, 1);
  histograms.ExpectTotalCount(
      "NaCl.Perf.Size.PexeNexeSizePct" + compiler_suffix, 1);
  histograms.ExpectTotalCount(
      "NaCl.Perf.PNaClLoadTime.LoadCompiler" + compiler_suffix, 1);
  histograms.ExpectTotalCount(
      "NaCl.Perf.PNaClLoadTime.CompileTime" + compiler_suffix, 1);
  histograms.ExpectTotalCount(
      "NaCl.Perf.PNaClLoadTime.CompileKBPerSec" + compiler_suffix, 1);
  histograms.ExpectTotalCount(
      "NaCl.Perf.PNaClLoadTime.PctCompiledWhenFullyDownloaded" +
          compiler_suffix,
      1);
  histograms.ExpectTotalCount(
      "NaCl.Perf.PNaClLoadTime.TotalUncachedTime" + compiler_suffix, 1);
  histograms.ExpectTotalCount(
      "NaCl.Perf.PNaClLoadTime.TotalUncachedKBPerSec" + compiler_suffix, 1);
  histograms.ExpectTotalCount("NaCl.Perf.PNaClCache.IsHit" + compiler_suffix,
                              1);
}

bool IsSubzeroSupportedForArch() {
  const char* arch = nacl::GetSandboxArch();
  if (strcmp(arch, "x86-32") == 0)
    return true;
  if (strcmp(arch, "x86-64") == 0)
    return true;
  if (strcmp(arch, "arm") == 0)
    return true;
  return false;
}

void FetchHistogramsFromChildProcesses() {
  // Support both traditional IPC and new "shared memory" channels.
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
}

NACL_BROWSER_TEST_F(NaClBrowserTest, SuccessfulLoadUMA, {
  base::HistogramTester histograms;
  // Load a NaCl module to generate UMA data.
  RunLoadTest(FILE_PATH_LITERAL("nacl_load_test.html"));

  // Make sure histograms from child processes have been accumulated in the
  // browser brocess.
  FetchHistogramsFromChildProcesses();

  // Did the plugin report success?
  histograms.ExpectUniqueSample("NaCl.LoadStatus.Plugin",
                                PP_NACL_ERROR_LOAD_SUCCESS, 1);

  // Did the sel_ldr report success?
  histograms.ExpectUniqueSample("NaCl.LoadStatus.SelLdr",
                                LOAD_OK, 1);

  // Check validation cache usage:
  if (IsAPnaclTest()) {
    // Should have received 5 validation queries:
    // - Three for the IRT: the app and both of the translator nexes use it.
    // - Two for the two PNaCl translator nexes.
    // The PNaCl app nexe comes from a delete-on-close temp file, so it
    // doesn't have a stable identity for validation caching. Overall, there
    // are 3 eligible nexes. The first 3 queries for these eligible nexes
    // are misses, and the latter two of the IRT queries are hits.
    histograms.ExpectBucketCount("NaCl.ValidationCache.Query",
                               nacl::NaClBrowser::CACHE_MISS, 3);
    histograms.ExpectBucketCount("NaCl.ValidationCache.Query",
                               nacl::NaClBrowser::CACHE_HIT, 2);
    // Should have received a cache setting afterwards (IRT set only once).
    histograms.ExpectUniqueSample("NaCl.ValidationCache.Set",
                                  nacl::NaClBrowser::CACHE_HIT, 3);
  } else {
    // For the open-web, only the IRT is considered a "safe" and
    // identity-cachable file. The nexes and .so files are not.
    // Should have one cache query for the IRT.
    histograms.ExpectUniqueSample("NaCl.ValidationCache.Query",
                                  nacl::NaClBrowser::CACHE_MISS, 1);
    // Should have received a cache setting afterwards for the IRT.
    histograms.ExpectUniqueSample("NaCl.ValidationCache.Set",
                                  nacl::NaClBrowser::CACHE_HIT, 1);
  }

  // Make sure we have other important histograms.
  if (!IsAPnaclTest()) {
    histograms.ExpectTotalCount("NaCl.Perf.StartupTime.LoadModule", 1);
    histograms.ExpectTotalCount("NaCl.Perf.StartupTime.Total", 1);
    histograms.ExpectTotalCount("NaCl.Perf.Size.Manifest", 1);
    histograms.ExpectTotalCount("NaCl.Perf.Size.Nexe", 1);
  } else {
    // There should be the total (suffix-free), and the LLC bucket.
    // Subzero is tested separately.
    CheckPNaClLoadUMAs(histograms, "");
    CheckPNaClLoadUMAs(histograms, ".LLC");
  }
})

IN_PROC_BROWSER_TEST_F(NaClBrowserTestPnaclSubzero, SuccessfulLoadUMA) {
  // Only test where Subzero is supported.
  if (!IsSubzeroSupportedForArch())
    return;

  base::HistogramTester histograms;
  // Run a load test that uses the -O0 NMF option.
  RunLoadTest(FILE_PATH_LITERAL("pnacl_options.html?use_nmf=o_0"));

  // Make sure histograms from child processes have been accumulated in the
  // browser brocess.
  FetchHistogramsFromChildProcesses();

  // Did the plugin report success?
  histograms.ExpectUniqueSample("NaCl.LoadStatus.Plugin",
                                PP_NACL_ERROR_LOAD_SUCCESS, 1);

  // Did the sel_ldr report success?
  histograms.ExpectUniqueSample("NaCl.LoadStatus.SelLdr", LOAD_OK, 1);

  // There should be the total (suffix-free), and the Subzero bucket.
  CheckPNaClLoadUMAs(histograms, "");
  CheckPNaClLoadUMAs(histograms, ".Subzero");
}

class NaClBrowserTestNewlibVcacheExtension:
      public NaClBrowserTestNewlibExtension {
 public:
  base::FilePath::StringType Variant() override {
    return FILE_PATH_LITERAL("extension_vcache_test/newlib");
  }
};

IN_PROC_BROWSER_TEST_F(NaClBrowserTestNewlibVcacheExtension,
                       ValidationCacheOfMainNexe) {
  base::HistogramTester histograms;
  // Hardcoded extension AppID that corresponds to the hardcoded
  // public key in the manifest.json file. We need to load the extension
  // nexe from the same origin, so we can't just try to load the extension
  // nexe as a mime-type handler from a non-extension URL.
  base::FilePath::StringType full_url =
      FILE_PATH_LITERAL("chrome-extension://cbcdidchbppangcjoddlpdjlenngjldk/")
      FILE_PATH_LITERAL("extension_validation_cache.html");
  RunNaClIntegrationTest(full_url, true);

  // Make sure histograms from child processes have been accumulated in the
  // browser brocess.
  FetchHistogramsFromChildProcesses();
  // Should have received 2 validation queries (one for IRT and one for NEXE),
  // and responded with a miss.
  histograms.ExpectBucketCount("NaCl.ValidationCache.Query",
                               nacl::NaClBrowser::CACHE_MISS, 2);
  // TOTAL should then be 2 queries so far.
  histograms.ExpectTotalCount("NaCl.ValidationCache.Query", 2);
  // Should have received a cache setting afterwards for IRT and nexe.
  histograms.ExpectBucketCount("NaCl.ValidationCache.Set",
                               nacl::NaClBrowser::CACHE_HIT, 2);

  // Load it again to hit the cache.
  RunNaClIntegrationTest(full_url, true);
  FetchHistogramsFromChildProcesses();
  // Should have received 2 more validation queries later (IRT and NEXE),
  // and responded with a hit.
  histograms.ExpectBucketCount("NaCl.ValidationCache.Query",
                               nacl::NaClBrowser::CACHE_HIT, 2);
  // TOTAL should then be 4 queries now.
  histograms.ExpectTotalCount("NaCl.ValidationCache.Query", 4);
  // Still only 2 settings.
  histograms.ExpectTotalCount("NaCl.ValidationCache.Set", 2);
}

class NaClBrowserTestGLibcVcacheExtension:
      public NaClBrowserTestGLibcExtension {
 public:
  base::FilePath::StringType Variant() override {
    return FILE_PATH_LITERAL("extension_vcache_test/glibc");
  }
};

IN_PROC_BROWSER_TEST_F(NaClBrowserTestGLibcVcacheExtension,
                       MAYBE_GLIBC(ValidationCacheOfMainNexe)) {
  // Make sure histograms from child processes have been accumulated in the
  // browser process.
  base::HistogramTester histograms;
  // Hardcoded extension AppID that corresponds to the hardcoded
  // public key in the manifest.json file. We need to load the extension
  // nexe from the same origin, so we can't just try to load the extension
  // nexe as a mime-type handler from a non-extension URL.
  base::FilePath::StringType full_url =
      FILE_PATH_LITERAL("chrome-extension://cbcdidchbppangcjoddlpdjlenngjldk/")
      FILE_PATH_LITERAL("extension_validation_cache.html");
  RunNaClIntegrationTest(full_url, true);

  // Should have received 9 (or 10 on arm) validation queries, which respond
  // with misses:
  //   - the IRT
  //   - ld.so (the initial nexe)
  //   - elf_loader.nexe (arm only)
  //   - main.nexe
  //   - libppapi_cpp.so
  //   - libpthread.so.9b15f6a6
  //   - libstdc++.so.6
  //   - libgcc_s.so.1
  //   - libc.so.9b15f6a6
  //   - libm.so.9b15f6a6
#if defined(ARCH_CPU_ARM_FAMILY)
  const int num_expected_binaries = 10;
#else
  const int num_expected_binaries = 9;
#endif

  histograms.ExpectBucketCount("NaCl.ValidationCache.Query",
                               nacl::NaClBrowser::CACHE_MISS,
                               num_expected_binaries);
  // TOTAL should then be N queries so far.
  histograms.ExpectTotalCount("NaCl.ValidationCache.Query",
                              num_expected_binaries);
  // Should have received a cache setting afterwards for IRT and nexe.
  histograms.ExpectBucketCount("NaCl.ValidationCache.Set",
                               nacl::NaClBrowser::CACHE_HIT,
                               num_expected_binaries);

  // Load it again to hit the cache.
  RunNaClIntegrationTest(full_url, true);
  // Should have received N more validation queries and responded with hits.
  histograms.ExpectBucketCount("NaCl.ValidationCache.Query",
                               nacl::NaClBrowser::CACHE_HIT,
                               num_expected_binaries);
  histograms.ExpectTotalCount("NaCl.ValidationCache.Query",
                              2 * num_expected_binaries);
  histograms.ExpectTotalCount("NaCl.ValidationCache.Set",
                              num_expected_binaries);
}

// Test that validation for the 2 (or 3) PNaCl translator nexes can be cached.
// This includes pnacl-llc.nexe, pnacl-ld.nexe, and possibly pnacl-sz.nexe.
IN_PROC_BROWSER_TEST_F(NaClBrowserTestPnacl, ValidationCacheOfTranslatorNexes) {
  const bool uses_subzero_with_o0 = IsSubzeroSupportedForArch();
  base::HistogramBase::Count subzero_o0_count = (uses_subzero_with_o0 ? 1 : 0);
  base::HistogramTester histograms;
  // Run a load test w/ one pexe cache identity.
  RunLoadTest(FILE_PATH_LITERAL("pnacl_options.html?use_nmf=o_0"));

  FetchHistogramsFromChildProcesses();
  // Should have received 5 validation queries:
  // - Three for the IRT: the app and both of the translator nexes use it.
  // - Two for the two PNaCl translator nexes.
  // - The PNaCl app nexe comes from a delete-on-close temp file, so it
  //   doesn't have a stable identity for validation caching.
  histograms.ExpectBucketCount("NaCl.ValidationCache.Query",
                               nacl::NaClBrowser::CACHE_MISS, 3);
  histograms.ExpectBucketCount("NaCl.ValidationCache.Query",
                               nacl::NaClBrowser::CACHE_HIT, 2);
  // Should have received a cache setting afterwards.
  histograms.ExpectUniqueSample("NaCl.ValidationCache.Set",
                               nacl::NaClBrowser::CACHE_HIT, 3);

  // Load the same pexe, but with a different cache identity.
  // This means that translation will actually be redone,
  // forcing the translators to be loaded a second time (but now with
  // cache hits!)
  RunLoadTest(FILE_PATH_LITERAL("pnacl_options.html?use_nmf=o_2"));

  // Should now have 5 more queries on top of the previous ones.
  histograms.ExpectTotalCount("NaCl.ValidationCache.Query", 10);
  // With the extra queries being cache hits (except that pnacl-llc.nexe will be
  // a miss if pnacl-sz.nexe was used the first time around).
  histograms.ExpectBucketCount("NaCl.ValidationCache.Query",
                               nacl::NaClBrowser::CACHE_HIT,
                               7 - subzero_o0_count);
  // No extra cache settings (except to add pnacl-llc.nexe if pnacl-sz.nexe was
  // used the first time around).
  histograms.ExpectUniqueSample("NaCl.ValidationCache.Set",
                                nacl::NaClBrowser::CACHE_HIT,
                                3 + subzero_o0_count);
}

// TODO(ncbray) convert the rest of nacl_uma.py (currently in the NaCl repo.)
// Test validation failures and crashes.

}  // namespace
