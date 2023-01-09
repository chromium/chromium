// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/autofill_and_password_manager_internals/internals_ui_handler.h"

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace {

class AutofillInternalsWebUIBrowserTest : public InProcessBrowserTest {
 public:
  content::EvalJsResult EvalJs(const std::string& code) {
    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return content::EvalJs(contents, code,
                           content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                           1 /* world_id */);
  }

  ::testing::AssertionResult ExecJs(const std::string& code) {
    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return content::ExecJs(contents, code,
                           content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                           1 /* world_id */);
  }

  void SpinRunLoop() {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(20));
    run_loop.Run();
  }
};

IN_PROC_BROWSER_TEST_F(AutofillInternalsWebUIBrowserTest, ResetCache) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("chrome://autofill-internals")));

  // Wait for reset-fake-button to become visible
  constexpr char kGetResetButtonDisplayStyle[] =
      "document.getElementById('reset-cache-fake-button').style.display";
  while ("inline" != EvalJs(kGetResetButtonDisplayStyle))
    SpinRunLoop();

  // Trigger reset button.
  constexpr char kClickResetButton[] =
      "document.getElementById('reset-cache-fake-button').click();";
  EXPECT_TRUE(ExecJs(kClickResetButton));

  // Wait for dialog to appear.
  constexpr char kDialogTextVisible[] =
      "document.getElementsByClassName('modal-dialog-text').length > 0";
  while (!EvalJs(kDialogTextVisible).ExtractBool())
    SpinRunLoop();

  // Check result text.
  constexpr char kDialogText[] =
      "document.getElementsByClassName('modal-dialog-text')[0].innerText";
  EXPECT_EQ(autofill::kCacheResetDone, EvalJs(kDialogText));

  // Close dialog.
  constexpr char kClickCloseButton[] =
      "document.getElementsByClassName('modal-dialog-close-button')[0]"
      ".click();";
  EXPECT_TRUE(ExecJs(kClickCloseButton));

  // Wait for dialog to disappear.
  while (EvalJs(kDialogTextVisible).ExtractBool())
    SpinRunLoop();
}

}  // namespace
