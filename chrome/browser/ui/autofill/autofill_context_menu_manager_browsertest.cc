// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_context_menu_manager.h"

#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/feedback/feedback_dialog.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_feedback_data.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_test.h"

namespace autofill {

class AutofillContextMenuManagerFeedbackUIBrowserTest
    : public InProcessBrowserTest {
 public:
  AutofillContextMenuManagerFeedbackUIBrowserTest() {
    feature_.InitWithFeatures(
        /*enabled_features=*/{features::
                                  kAutofillShowManualFallbackInContextMenu,
                              features::kAutofillFeedback},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    render_view_context_menu_ = std::make_unique<TestRenderViewContextMenu>(
        *web_contents()->GetPrimaryMainFrame(), content::ContextMenuParams());
    render_view_context_menu_->Init();
    autofill_context_menu_manager_ =
        std::make_unique<AutofillContextMenuManager>(
            nullptr, render_view_context_menu_.get(), nullptr, nullptr);

    browser()->profile()->GetPrefs()->SetBoolean(prefs::kUserFeedbackAllowed,
                                                 true);
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  std::unique_ptr<TestRenderViewContextMenu> render_view_context_menu_;
  std::unique_ptr<AutofillContextMenuManager> autofill_context_menu_manager_;
  base::test::ScopedFeatureList feature_;
};

// Awaits for the feedback dialog to be active. `callback` gets triggered
// once the dialog is shown.
void EnsureFeedbackAppUIShown(FeedbackDialog* feedback_dialog,
                              base::OnceClosure callback) {
  auto* widget = feedback_dialog->GetWidget();
  ASSERT_NE(nullptr, widget);
  if (widget->IsActive()) {
    std::move(callback).Run();
  } else {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&EnsureFeedbackAppUIShown, feedback_dialog,
                       std::move(callback)),
        base::Seconds(1));
  }
}

IN_PROC_BROWSER_TEST_F(AutofillContextMenuManagerFeedbackUIBrowserTest,
                       FeedbackUIIsRequested) {
  base::HistogramTester histogram_tester;
  // Executing autofill feedback command opens the Feedback UI.
  autofill_context_menu_manager_->ExecuteCommand(
      AutofillContextMenuManager::CommandId(
          IDC_CONTENT_CONTEXT_AUTOFILL_FEEDBACK));

  // Checks that feedback form was requested.
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
}

IN_PROC_BROWSER_TEST_F(AutofillContextMenuManagerFeedbackUIBrowserTest,
                       CloseTabWhileUIIsOpenShouldNotCrash) {
  autofill_context_menu_manager_->ExecuteCommand(
      AutofillContextMenuManager::CommandId(
          IDC_CONTENT_CONTEXT_AUTOFILL_FEEDBACK));

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  tab->Close();
}

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(AutofillContextMenuManagerFeedbackUIBrowserTest,
                       DisplaysFeedbackDialogUI) {
  base::RunLoop run_loop;
  // Test that no feedback dialog exists.
  ASSERT_EQ(nullptr, FeedbackDialog::GetInstanceForTest());

  autofill_context_menu_manager_->ExecuteCommand(
      AutofillContextMenuManager::CommandId(
          IDC_CONTENT_CONTEXT_AUTOFILL_FEEDBACK));

  FeedbackDialog* feedback_dialog = FeedbackDialog::GetInstanceForTest();
  // Test that a feedback dialog object has been created.
  ASSERT_NE(nullptr, feedback_dialog);

  // The feedback app starts invisible until after a screenshot has been taken
  // via JS on the UI side. Afterward, JS will send a request to show the app
  // window via a message handler.
  EnsureFeedbackAppUIShown(feedback_dialog, run_loop.QuitClosure());
  run_loop.Run();

  // Test that the feedback app is visible now.
  EXPECT_TRUE(feedback_dialog->GetWidget()->IsVisible());

  // Close the feedback dialog.
  feedback_dialog->GetWidget()->Close();
}

IN_PROC_BROWSER_TEST_F(AutofillContextMenuManagerFeedbackUIBrowserTest,
                       FeedbackDialogArgsAutofillMetadata) {
  AutofillManager* manager = ContentAutofillDriver::GetForRenderFrameHost(
                                 web_contents()->GetPrimaryMainFrame())
                                 ->autofill_manager();
  ASSERT_TRUE(manager);
  std::string expected_metadata;
  base::JSONWriter::Write(data_logs::FetchAutofillFeedbackData(manager),
                          &expected_metadata);

  // Test that none feedback dialog exists.
  ASSERT_EQ(nullptr, FeedbackDialog::GetInstanceForTest());

  // Display feedback dialog.
  autofill_context_menu_manager_->ExecuteCommand(
      AutofillContextMenuManager::CommandId(
          IDC_CONTENT_CONTEXT_AUTOFILL_FEEDBACK));

  ui::WebDialogDelegate* feedback_dialog = FeedbackDialog::GetInstanceForTest();
  // Test that a feedback dialog object has been created.
  ASSERT_NE(nullptr, feedback_dialog);

  // Extract autofill metadata from dialog arguments and check for correctness.
  std::string dialog_args_str = feedback_dialog->GetDialogArgs();
  absl::optional<base::Value> value = base::JSONReader::Read(dialog_args_str);
  ASSERT_TRUE(value.has_value() && value->is_dict());
  const std::string* autofill_metadata =
      value->GetDict().FindString("autofillMetadata");
  ASSERT_TRUE(autofill_metadata);
  EXPECT_EQ(*autofill_metadata, expected_metadata);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace autofill
