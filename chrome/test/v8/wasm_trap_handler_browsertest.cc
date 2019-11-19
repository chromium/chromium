// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These tests focus on Wasm out of bounds behavior to make sure trap-based
// bounds checks work when integrated with all of Chrome.

#include "base/base_switches.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"

namespace {
// |kIsTrapHandlerSupported| indicates whether the trap handler is supported
// (i.e. allowed to be enabled) on the currently platform. Currently we only
// support non-Android, Linux x64, and Windows x64. In the future more platforms
// will be supported. Though this file is a browser test that is not built on
// Android.
#if defined(OS_LINUX) && defined(ARCH_CPU_X86_64)
constexpr bool kIsTrapHandlerSupported = true;
#elif defined(OS_WIN) && defined(ARCH_CPU_X86_64)
constexpr bool kIsTrapHandlerSupported = true;
#elif defined(OS_MACOSX) && defined(ARCH_CPU_X86_64)
constexpr bool kIsTrapHandlerSupported = true;
#else
constexpr bool kIsTrapHandlerSupported = false;
#endif

class WasmTrapHandlerBrowserTest : public InProcessBrowserTest {
 public:
  WasmTrapHandlerBrowserTest() {}
  ~WasmTrapHandlerBrowserTest() override {}

 protected:
  void RunJSTest(const std::string& js) const {
    auto* const tab = browser()->tab_strip_model()->GetActiveWebContents();
    bool result = false;

    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(tab, js, &result));
    ASSERT_TRUE(result);
  }

  void RunJSTestAndEnsureTrapHandlerRan(const std::string& js) const {
    if (IsTrapHandlerEnabled()) {
      const auto* get_fault_count =
          "domAutomationController.send(%GetWasmRecoveredTrapCount())";
      int original_count = 0;
      auto* const tab = browser()->tab_strip_model()->GetActiveWebContents();
      ASSERT_TRUE(content::ExecuteScriptAndExtractInt(tab, get_fault_count,
                                                      &original_count));
      ASSERT_NO_FATAL_FAILURE(RunJSTest(js));
      int new_count = 0;
      ASSERT_TRUE(content::ExecuteScriptAndExtractInt(tab, get_fault_count,
                                                      &new_count));
      ASSERT_GT(new_count, original_count);
    } else {
      ASSERT_NO_FATAL_FAILURE(RunJSTest(js));
    }
  }

  // Calls %IsWasmTrapHandlerEnabled and returns the result.
  bool IsTrapHandlerEnabled() const {
    bool is_trap_handler_enabled = false;
    const char* script =
        "domAutomationController.send(%IsWasmTrapHandlerEnabled())";
    auto* const tab = browser()->tab_strip_model()->GetActiveWebContents();
    CHECK(content::ExecuteScriptAndExtractBool(tab, script,
                                               &is_trap_handler_enabled));
    return is_trap_handler_enabled;
  }

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override {
// kEnableCrashReporterForTesting only exists on POSIX systems
#if defined(OS_POSIX)
    command_line->AppendSwitch(switches::kEnableCrashReporterForTesting);
#endif
    command_line->AppendSwitchASCII(switches::kJavaScriptFlags,
                                    "--allow-natives-syntax");
  }

  DISALLOW_COPY_AND_ASSIGN(WasmTrapHandlerBrowserTest);
};

IN_PROC_BROWSER_TEST_F(WasmTrapHandlerBrowserTest, OutOfBounds) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const auto& url = embedded_test_server()->GetURL("/wasm/out_of_bounds.html");
  ui_test_utils::NavigateToURL(browser(), url);

  ASSERT_NO_FATAL_FAILURE(RunJSTest("peek_in_bounds()"));
  ASSERT_NO_FATAL_FAILURE(
      RunJSTestAndEnsureTrapHandlerRan("peek_out_of_bounds()"));
  ASSERT_NO_FATAL_FAILURE(RunJSTestAndEnsureTrapHandlerRan(
      "peek_out_of_bounds_grow_memory_from_zero_js()"));
  ASSERT_NO_FATAL_FAILURE(
      RunJSTestAndEnsureTrapHandlerRan("peek_out_of_bounds_grow_memory_js()"));
  ASSERT_NO_FATAL_FAILURE(RunJSTestAndEnsureTrapHandlerRan(
      "peek_out_of_bounds_grow_memory_from_zero_wasm()"));
  ASSERT_NO_FATAL_FAILURE(RunJSTestAndEnsureTrapHandlerRan(
      "peek_out_of_bounds_grow_memory_wasm()"));

  ASSERT_NO_FATAL_FAILURE(RunJSTest("poke_in_bounds()"));
  ASSERT_NO_FATAL_FAILURE(
      RunJSTestAndEnsureTrapHandlerRan("poke_out_of_bounds()"));
  ASSERT_NO_FATAL_FAILURE(RunJSTestAndEnsureTrapHandlerRan(
      "poke_out_of_bounds_grow_memory_from_zero_js()"));
  ASSERT_NO_FATAL_FAILURE(
      RunJSTestAndEnsureTrapHandlerRan("poke_out_of_bounds_grow_memory_js()"));
  ASSERT_NO_FATAL_FAILURE(RunJSTestAndEnsureTrapHandlerRan(
      "poke_out_of_bounds_grow_memory_from_zero_wasm()"));
  ASSERT_NO_FATAL_FAILURE(RunJSTestAndEnsureTrapHandlerRan(
      "poke_out_of_bounds_grow_memory_wasm()"));
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
  ignore_result(is_trap_handler_enabled);
  return;
#endif

  ASSERT_EQ(is_trap_handler_enabled,
            kIsTrapHandlerSupported && base::FeatureList::IsEnabled(
                                           features::kWebAssemblyTrapHandler));
}
}  // namespace
