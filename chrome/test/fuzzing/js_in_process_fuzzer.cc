// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/fuzzing/in_process_fuzzer.h"
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
  JsInProcessFuzzer() = default;
  void SetUpOnMainThread() override;

  int Fuzz(const uint8_t* data, size_t size) override;
};

REGISTER_IN_PROCESS_FUZZER(JsInProcessFuzzer)

namespace {

// We have a timeout to avoid JavaScript infinite loops hanging the
// fuzzer. Empirically, valid JS cases complete locally well within 2
// seconds so allow 8 seconds to account for slowness on fuzzing
// infrastructure.
constexpr base::TimeDelta kJsExecutionTimeout = base::Seconds(8);
constexpr std::string_view kBlankHtmlPage =
    "<html><head><title>Test page</title></head>"
    "<body><p>Test text.</p></body></html>";

}  // namespace

void JsInProcessFuzzer::SetUpOnMainThread() {
  InProcessFuzzer::SetUpOnMainThread();
  std::string url_string = "data:text/html;charset=utf-8,";
  const bool kUsePlus = false;
  url_string.append(base::EscapeQueryParamValue(kBlankHtmlPage, kUsePlus));
  CHECK(ui_test_utils::NavigateToURL(browser(), GURL(url_string)));
}

int JsInProcessFuzzer::Fuzz(const uint8_t* data, size_t size) {
  std::string_view js_str(reinterpret_cast<const char*>(data), size);
  base::RunLoop run_loop;
  base::OneShotTimer timer;
  bool valid_input = true;
  base::RepeatingCallback<void()> run_fuzz_case_lambda =
      base::BindLambdaForTesting([&]() {
        std::u16string js_str16 = base::UTF8ToUTF16(std::string_view(js_str));
        // End the run loop either when the JS finishes or 2 seconds expires
        timer.Start(FROM_HERE, kJsExecutionTimeout,
                    base::BindLambdaForTesting([&]() {
                      // If we hit a timeout executing this test case, it's
                      // probably a JavaScript infinite loop. If we leave it for
                      // 30+ seconds then the RunLoop will crash, and the fuzzer
                      // engine will regard that as a failure. Instead, we exit
                      // the entire browser. Centipede will assume that the
                      // problem is that the shared memory isn't big enough.
                      // This is obviously a little inefficient, but it's
                      // unclear that there's a better way to do this. If we
                      // didn't run these fuzzers in --single-process mode then
                      // we could Shutdown() the RenderProcessHost, but that's
                      // not an option right now.
                      LOG(INFO) << "Timeout hit, exiting the whole browser on "
                                   "next iteration.";
                      DeclareInfiniteLoop();
                      valid_input = false;
                      run_loop.Quit();
                    }));
        content::WebContents* contents =
            browser()->tab_strip_model()->GetActiveWebContents();
        content::RenderFrameHost* rfh = contents->GetPrimaryMainFrame();
        // Execute JS within an existing page - that's the essence of this
        // fuzzer and why it's much quicker than html_in_process_fuzzer
        // or page_load_in_process_fuzzer.
        // We use this asynchronous function rather than (for instance)
        // content::ExecJs so that we can apply the timeout above.
        // We use the "WithUserGesture" version so that this can
        // in theory explore APIs which are gated behind that restriction
        // (subject to future developments with dictionaries, corpora, etc.)
        rfh->ExecuteJavaScriptWithUserGestureForTests(
            js_str16, base::BindLambdaForTesting([&](base::Value) {
              timer.Stop();
              run_loop.Quit();
            }));
      });
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_fuzz_case_lambda);
  run_loop.Run();
  return valid_input ? 0 : -1;
}
