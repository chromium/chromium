// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_context_menu_manager.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_test.h"

namespace autofill {

class AutofillContextMenuManagerFeedbackUILacrosBrowserTest
    : public InProcessBrowserTest {
 public:
  AutofillContextMenuManagerFeedbackUILacrosBrowserTest() = default;
  void SetUp() override {
    StartUniqueAshChrome(
        /*enabled_features=*/{"AutofillFeedback"},
        /*disabled_fetures=*/{}, /*additional_cmd_switches=*/{},
        "crbug.com/1473402 The test enabled the feature "
        "AutofillFeedback (disabled by default)");

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    render_view_context_menu_ = std::make_unique<TestRenderViewContextMenu>(
        *web_contents()->GetPrimaryMainFrame(), content::ContextMenuParams());
    render_view_context_menu_->Init();
    autofill_context_menu_manager_ =
        std::make_unique<AutofillContextMenuManager>(
            nullptr, render_view_context_menu_.get(), nullptr);

    browser()->profile()->GetPrefs()->SetBoolean(prefs::kUserFeedbackAllowed,
                                                 true);
  }

  void TearDownOnMainThread() override {
    autofill_context_menu_manager_.reset();

    if (IsCloseAndWaitAshBrowserWindowApisSupported()) {
      CloseAllAshBrowserWindows();
    }

    InProcessBrowserTest::TearDownOnMainThread();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  void ExecuteAutofillFeedbackCommand() {
    // Executing autofill feedback command opens the Feedback UI.
    autofill_context_menu_manager_->ExecuteCommand(
        IDC_CONTENT_CONTEXT_AUTOFILL_FEEDBACK);
    // Verify the Feedback UI opens in Ash.
    if (IsCloseAndWaitAshBrowserWindowApisSupported()) {
      // There has not been a convenient way to verify a specific UI in Ash from
      // Lacros yet. Therefore, we just verify there is an Ash window opened
      // since Feedback UI is a SWA.
      WaitUntilAtLeastOneAshBrowserWindowOpen();
    }
  }

  std::unique_ptr<TestRenderViewContextMenu> render_view_context_menu_;
  std::unique_ptr<AutofillContextMenuManager> autofill_context_menu_manager_;

 private:
  test::AutofillBrowserTestEnvironment autofill_test_environment_;
};

IN_PROC_BROWSER_TEST_F(AutofillContextMenuManagerFeedbackUILacrosBrowserTest,
                       FeedbackUIIsRequested) {
  base::HistogramTester histogram_tester;
  // Executing autofill feedback command opens the Feedback UI.
  ExecuteAutofillFeedbackCommand();
  // Checks that feedback form was requested.
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
}

IN_PROC_BROWSER_TEST_F(AutofillContextMenuManagerFeedbackUILacrosBrowserTest,
                       CloseTabWhileUIIsOpenShouldNotCrash) {
  ExecuteAutofillFeedbackCommand();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  tab->Close();
}

}  // namespace autofill
