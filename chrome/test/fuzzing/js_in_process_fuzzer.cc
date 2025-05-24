// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string_view>

#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/fuzzing/in_process_fuzzer.h"
#include "chrome/test/fuzzing/in_process_fuzzer_buildflags.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test_utils.h"

// This is an example use of the InProcessFuzzer framework.
// It runs arbitrary JS within the context of an existing
// loaded page, which is much quicker than loading a new
// page each time. It has no awareness of JS syntax so
// it's unlikely to be an effective fuzzer; future
// developments may feed it a useful corpus or dictionary
// or add a mutator. This is a first step in that direction.

class JsInProcessFuzzer : public InProcessFuzzer {
 public:
  JsInProcessFuzzer();
  void SetUpOnMainThread() override;
  base::CommandLine::StringVector GetChromiumCommandLineArguments() override;

  int Fuzz(const uint8_t* data, size_t size) override;
};

REGISTER_IN_PROCESS_FUZZER(JsInProcessFuzzer)

namespace {

// We have a timeout to avoid JavaScript infinite loops hanging the
// fuzzer. Empirically, valid JS cases complete locally well within 2
// seconds so allow 8 seconds to account for slowness on fuzzing
// infrastructure.

#if BUILDFLAG(IS_FUZZILLI)
// Fuzzilli handles timeouts by itself so that it detects when there are
// infinite loops.
constexpr std::optional<base::TimeDelta> kJsExecutionTimeout = std::nullopt;
constexpr RunLoopTimeoutBehavior kJsRunLoopTimeoutBehavior =
    RunLoopTimeoutBehavior::kDefault;
#else
constexpr std::optional<base::TimeDelta> kJsExecutionTimeout = base::Seconds(8);
constexpr RunLoopTimeoutBehavior kJsRunLoopTimeoutBehavior =
    RunLoopTimeoutBehavior::kDeclareInfiniteLoop;
#endif

constexpr std::string_view kBlankHtmlPage =
    "<html><head><title>Test page</title></head>"
    "<body><p>Test text.</p></body></html>";

}  // namespace

JsInProcessFuzzer::JsInProcessFuzzer()
    : InProcessFuzzer(InProcessFuzzerOptions{
          .run_loop_timeout_behavior = kJsRunLoopTimeoutBehavior,
          .run_loop_timeout = kJsExecutionTimeout,
      }) {}

void JsInProcessFuzzer::SetUpOnMainThread() {
  InProcessFuzzer::SetUpOnMainThread();
  std::string url_string = "data:text/html;charset=utf-8,";
  const bool kUsePlus = false;
  url_string.append(base::EscapeQueryParamValue(kBlankHtmlPage, kUsePlus));
  CHECK(ui_test_utils::NavigateToURL(browser(), GURL(url_string)));
#if BUILDFLAG(IS_FUZZILLI)
  // Fuzzilli needs to see this. Unfortunately, we install a signal handler at
  // //content/public/test/browser_test_base.cc that exits when one of those
  // signals occur. Disabling it allows for Fuzzilli to see them.
  signal(SIGSEGV, SIG_DFL);
  signal(SIGTERM, SIG_DFL);
#endif
}

base::CommandLine::StringVector
JsInProcessFuzzer::GetChromiumCommandLineArguments() {
#if BUILDFLAG(IS_FUZZILLI)
  char template_str[] = "/tmp/fuzzilli_tmp/XXXXXX";
  char* path_dir = mkdtemp(template_str);
  std::string user_data_dir = "--user-data-dir=" + std::string(path_dir);
#endif
  return {
      FILE_PATH_LITERAL("--js-flags='--jit-fuzzing --allow-natives-syntax "
                        "--expose-gc --fuzzing --future --harmony'"),
#if BUILDFLAG(IS_FUZZILLI)
      // This was caused by some issues with disks filling up fast, because
      // Fuzzilli restarts the binary very frequently.
      user_data_dir,
#endif
  };
}

int JsInProcessFuzzer::Fuzz(const uint8_t* data, size_t size) {
  std::string_view js_str(reinterpret_cast<const char*>(data), size);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* rfh = contents->GetPrimaryMainFrame();
  // Execute JS within an existing page - that's the essence of this
  // fuzzer and why it's much quicker than html_in_process_fuzzer
  // or page_load_in_process_fuzzer.
  // We use a synchronous function because we set the InProcessFuzzer's
  // kDeclareInfiniteLoop behaviour in case of infinite loops.
  // We invoke ExecJs with default options, which turns on user gestures in JS
  // so that this can in theory explore APIs which are gated behind that
  // restriction (subject to future developments with dictionaries, corpora,
  // etc.)
  testing::AssertionResult res = content::ExecJs(rfh, js_str);
#if BUILDFLAG(IS_FUZZILLI)
  // Fuzzilli needs to know when an exception was uncaught.
  if (!res) {
    return -1;
  }
#endif
  return 0;
}
