// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These tests focus on Wasm out of bounds behavior to make sure trap-based
// bounds checks work when integrated with all of Chrome.

#include <tuple>

#include "base/base_switches.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/switches.h"

namespace {
// |kIsTrapHandlerSupported| indicates whether the trap handler is supported
// (i.e. allowed to be enabled) on the currently platform. Currently we only
// support non-Android, Linux x64 and AArch64, Windows x64 and Mac.
// In the future more platforms will be supported. Though this file is a
// browser test that is not built on Android.
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && \
    (defined(ARCH_CPU_X86_64) || defined(ARCH_CPU_ARM64))
constexpr bool kIsTrapHandlerSupported = true;
#elif BUILDFLAG(IS_WIN) && defined(ARCH_CPU_X86_64)
constexpr bool kIsTrapHandlerSupported = true;
#elif BUILDFLAG(IS_MAC) && (defined(ARCH_CPU_X86_64) || defined(ARCH_CPU_ARM64))
constexpr bool kIsTrapHandlerSupported = true;
#else
constexpr bool kIsTrapHandlerSupported = false;
#endif

class WasmTrapHandlerBrowserTest : public InProcessBrowserTest {
 public:
  WasmTrapHandlerBrowserTest() {}

  WasmTrapHandlerBrowserTest(const WasmTrapHandlerBrowserTest&) = delete;
  WasmTrapHandlerBrowserTest& operator=(const WasmTrapHandlerBrowserTest&) =
      delete;

  ~WasmTrapHandlerBrowserTest() override {}

 protected:
  void RunJSTest(const std::string& js) const {
    auto* const tab = browser()->tab_strip_model()->GetActiveWebContents();

    ASSERT_EQ(true, content::EvalJs(tab, js));
  }

  void RunJSTestAndEnsureTrapHandlerRan(const std::string& js,
                                        int num_oob_loads,
                                        int num_oob_stores) const {
    if (IsTrapHandlerEnabled()) {
      int original_count = GetRecoveredTrapCount();
      ASSERT_NO_FATAL_FAILURE(RunJSTest(js));
      int new_count = GetRecoveredTrapCount();
      // Out-of-bounds writes, on AArch64 Linux platforms, can perform partial
      // writes. On these platforms, we do not use trap handlers to perform
      // bounds checks on writes. So, for tests that are attempting to access
      // OoB memory using a store, we only expect the trap count to increase
      // when trap handlers are used to perform bounds checks on stores.
      int expected_count = IsPartialOOBWriteNoop()
                               ? original_count + num_oob_loads + num_oob_stores
                               : original_count + num_oob_loads;
      ASSERT_EQ(new_count, expected_count);
    } else {
      ASSERT_NO_FATAL_FAILURE(RunJSTest(js));
    }
  }

  // Calls %GetWasmRecoveredTrapCount and returns the result.
  int GetRecoveredTrapCount() const {
    const char* script = "%GetWasmRecoveredTrapCount()";
    auto* const tab = browser()->tab_strip_model()->GetActiveWebContents();
    return content::EvalJs(tab, script).ExtractInt();
  }

  // Calls %IsWasmPartialOOBWriteNoop and returns the result.
  bool IsPartialOOBWriteNoop() const {
    const char* script = "%IsWasmPartialOOBWriteNoop()";
    auto* const tab = browser()->tab_strip_model()->GetActiveWebContents();
    return content::EvalJs(tab, script).ExtractBool();
  }

  // Calls %IsWasmTrapHandlerEnabled and returns the result.
  bool IsTrapHandlerEnabled() const {
    const char* script = "%IsWasmTrapHandlerEnabled()";
    auto* const tab = browser()->tab_strip_model()->GetActiveWebContents();
    return content::EvalJs(tab, script).ExtractBool();
  }

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override {
// kEnableCrashReporterForTesting only exists on POSIX systems
#if BUILDFLAG(IS_POSIX)
    command_line->AppendSwitch(switches::kEnableCrashReporterForTesting);
#endif
    command_line->AppendSwitchASCII(blink::switches::kJavaScriptFlags,
                                    "--allow-natives-syntax");
  }
};

IN_PROC_BROWSER_TEST_F(WasmTrapHandlerBrowserTest, OutOfBounds) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const auto& url = embedded_test_server()->GetURL("/wasm/out_of_bounds.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_NO_FATAL_FAILURE(RunJSTest("peek_in_bounds()"));
  ASSERT_NO_FATAL_FAILURE(
      RunJSTestAndEnsureTrapHandlerRan("peek_out_of_bounds()", 6, 0));
  ASSERT_NO_FATAL_FAILURE(RunJSTestAndEnsureTrapHandlerRan(
      "peek_out_of_bounds_grow_memory_from_zero_js()", 1, 0));
  ASSERT_NO_FATAL_FAILURE(RunJSTestAndEnsureTrapHandlerRan(
      "peek_out_of_bounds_grow_memory_js()", 1, 0));
  ASSERT_NO_FATAL_FAILURE(RunJSTestAndEnsureTrapHandlerRan(
      "peek_out_of_bounds_grow_memory_from_zero_wasm()", 1, 0));
  ASSERT_NO_FATAL_FAILURE(RunJSTestAndEnsureTrapHandlerRan(
      "peek_out_of_bounds_grow_memory_wasm()", 1, 0));

  ASSERT_NO_FATAL_FAILURE(RunJSTest("poke_in_bounds()"));
  ASSERT_NO_FATAL_FAILURE(
      RunJSTestAndEnsureTrapHandlerRan("poke_out_of_bounds()", 0, 6));
  ASSERT_NO_FATAL_FAILURE(RunJSTestAndEnsureTrapHandlerRan(
      "poke_out_of_bounds_grow_memory_from_zero_js()", 0, 1));
  ASSERT_NO_FATAL_FAILURE(RunJSTestAndEnsureTrapHandlerRan(
      "poke_out_of_bounds_grow_memory_js()", 0, 1));
  ASSERT_NO_FATAL_FAILURE(RunJSTestAndEnsureTrapHandlerRan(
      "poke_out_of_bounds_grow_memory_from_zero_wasm()", 0, 1));
  ASSERT_NO_FATAL_FAILURE(RunJSTestAndEnsureTrapHandlerRan(
      "poke_out_of_bounds_grow_memory_wasm()", 0, 1));
}

IN_PROC_BROWSER_TEST_F(WasmTrapHandlerBrowserTest,
                       TrapHandlerCorrectlyConfigured) {
  const bool is_trap_handler_enabled = IsTrapHandlerEnabled();
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
    defined(THREAD_SANITIZER) || defined(LEAK_SANITIZER) ||    \
    defined(UNDEFINED_SANITIZER)
  // Sanitizers may prevent signal handler installation and thereby trap handler
  // setup. As there is no easy way to test if signal handler installation is
  // possible, we disable this test for sanitizers.
  std::ignore = is_trap_handler_enabled;
  return;
#endif

  ASSERT_EQ(is_trap_handler_enabled,
            kIsTrapHandlerSupported && base::FeatureList::IsEnabled(
                                           features::kWebAssemblyTrapHandler));
}
}  // namespace
