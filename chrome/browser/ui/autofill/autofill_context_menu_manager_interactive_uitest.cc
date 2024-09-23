// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_context_menu_manager.h"

#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/webui/feedback/feedback_dialog.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/core/browser/autofill_feedback_data.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_test.h"

namespace autofill {
namespace {

#if !BUILDFLAG(IS_CHROMEOS)
// Generates a ContextMenuParams for the Autofill context menu options.
content::ContextMenuParams CreateContextMenuParams(
    std::optional<autofill::FormRendererId> form_renderer_id = std::nullopt,
    autofill::FieldRendererId field_render_id = autofill::FieldRendererId(0)) {
  content::ContextMenuParams rv;
  rv.is_editable = true;
  rv.page_url = GURL("http://test.page/");
  rv.form_control_type = blink::mojom::FormControlType::kInputText;
  if (form_renderer_id) {
    rv.form_renderer_id = form_renderer_id->value();
  }
  rv.field_renderer_id = field_render_id.value();
  return rv;
}

class TestAutofillManager : public BrowserAutofillManager {
 public:
  explicit TestAutofillManager(ContentAutofillDriver* driver)
      : BrowserAutofillManager(driver, "en-US") {}

  testing::AssertionResult WaitForFormsSeen(int min_num_awaited_calls) {
    return forms_seen_waiter_.Wait(min_num_awaited_calls);
  }

 private:
  TestAutofillManagerWaiter forms_seen_waiter_{
      *this,
      {AutofillManagerEvent::kFormsSeen}};
};

class AutofillContextMenuManagerFeedbackUIBrowserTest
    : public InProcessBrowserTest {
 public:
  AutofillContextMenuManagerFeedbackUIBrowserTest() = default;

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
    render_view_context_menu_.reset();

    InProcessBrowserTest::TearDownOnMainThread();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  TestAutofillManager* GetAutofillManager() {
    return autofill_manager_injector_[web_contents()->GetPrimaryMainFrame()];
  }

 protected:
  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  std::unique_ptr<TestRenderViewContextMenu> render_view_context_menu_;
  std::unique_ptr<AutofillContextMenuManager> autofill_context_menu_manager_;
  TestAutofillManagerInjector<TestAutofillManager> autofill_manager_injector_;
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
      IDC_CONTENT_CONTEXT_AUTOFILL_FEEDBACK);

  // Checks that feedback form was requested.
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
}

IN_PROC_BROWSER_TEST_F(AutofillContextMenuManagerFeedbackUIBrowserTest,
                       CloseTabWhileUIIsOpenShouldNotCrash) {
  autofill_context_menu_manager_->ExecuteCommand(
      IDC_CONTENT_CONTEXT_AUTOFILL_FEEDBACK);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  tab->Close();
}

IN_PROC_BROWSER_TEST_F(AutofillContextMenuManagerFeedbackUIBrowserTest,
                       DisplaysFeedbackDialogUI) {
  base::RunLoop run_loop;
  // Test that no feedback dialog exists.
  ASSERT_EQ(nullptr, FeedbackDialog::GetInstanceForTest());

  autofill_context_menu_manager_->ExecuteCommand(
      IDC_CONTENT_CONTEXT_AUTOFILL_FEEDBACK);

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

// Regression test for crbug.com/1493774.
IN_PROC_BROWSER_TEST_F(AutofillContextMenuManagerFeedbackUIBrowserTest,
                       TabMoveToOtherBrowserDoesNotCrash) {
  // Create another browser.
  Browser* other_browser = CreateBrowser(browser()->profile());

  // Move the tab to the other browser.
  other_browser->tab_strip_model()->InsertDetachedTabAt(
      0, browser()->tab_strip_model()->DetachTabAtForInsertion(0),
      AddTabTypes::ADD_ACTIVE);
  ASSERT_EQ(other_browser->tab_strip_model()->count(), 2);

  // Close the first browser.
  CloseBrowserSynchronously(browser());
}

IN_PROC_BROWSER_TEST_F(AutofillContextMenuManagerFeedbackUIBrowserTest,
                       FeedbackDialogArgsAutofillMetadata) {
  std::string expected_metadata;
  base::JSONWriter::Write(
      data_logs::FetchAutofillFeedbackData(GetAutofillManager()),
      &expected_metadata);

  // Test that none feedback dialog exists.
  ASSERT_EQ(nullptr, FeedbackDialog::GetInstanceForTest());

  // Display feedback dialog.
  autofill_context_menu_manager_->ExecuteCommand(
      IDC_CONTENT_CONTEXT_AUTOFILL_FEEDBACK);

  ui::WebDialogDelegate* feedback_dialog = FeedbackDialog::GetInstanceForTest();
  // Test that a feedback dialog object has been created.
  ASSERT_NE(nullptr, feedback_dialog);

  // Extract autofill metadata from dialog arguments and check for correctness.
  std::string dialog_args_str = feedback_dialog->GetDialogArgs();
  std::optional<base::Value> value = base::JSONReader::Read(dialog_args_str);
  ASSERT_TRUE(value.has_value() && value->is_dict());
  const std::string* autofill_metadata =
      value->GetDict().FindString("autofillMetadata");
  ASSERT_TRUE(autofill_metadata);
  EXPECT_EQ(*autofill_metadata, expected_metadata);
}

IN_PROC_BROWSER_TEST_F(AutofillContextMenuManagerFeedbackUIBrowserTest,
                       IncludesTriggerFormAndFieldSignatures) {
  content::RenderFrameHost* rfh = web_contents()->GetPrimaryMainFrame();
  LocalFrameToken frame_token(rfh->GetFrameToken().value());
  FormData form = test::CreateFormDataForFrame(
      test::CreateTestAddressFormData(), frame_token);
  GetAutofillManager()->OnFormsSeen(
      /*updated_forms=*/{form},
      /*removed_forms=*/{});
  GetAutofillManager()->WaitForFormsSeen(1);
  ASSERT_TRUE(GetAutofillManager()->FindCachedFormById(form.global_id()));

  // Set up expected trigger form and field signatures.
  std::string expected_metadata;
  base::Value::Dict extra_logs;
  auto form_structure = std::make_unique<FormStructure>(form);
  extra_logs.Set("triggerFormSignature", form_structure->FormSignatureAsStr());
  extra_logs.Set("triggerFieldSignature",
                 form_structure->field(0)->FieldSignatureAsStr());
  base::JSONWriter::Write(data_logs::FetchAutofillFeedbackData(
                              GetAutofillManager(), std::move(extra_logs)),
                          &expected_metadata);

  // Set up context menu params with the correct trigger form and field.
  autofill_context_menu_manager_->set_params_for_testing(
      CreateContextMenuParams(form.global_id().renderer_id,
                              form.fields()[0].global_id().renderer_id));
  // Display feedback dialog.
  autofill_context_menu_manager_->ExecuteCommand(
      IDC_CONTENT_CONTEXT_AUTOFILL_FEEDBACK);

  ui::WebDialogDelegate* feedback_dialog = FeedbackDialog::GetInstanceForTest();
  // Test that a feedback dialog object has been created.
  ASSERT_NE(nullptr, feedback_dialog);

  // Extract autofill metadata from dialog arguments and check for correctness.
  std::string dialog_args_str = feedback_dialog->GetDialogArgs();
  std::optional<base::Value> value = base::JSONReader::Read(dialog_args_str);
  ASSERT_TRUE(value.has_value() && value->is_dict());
  const std::string* autofill_metadata =
      value->GetDict().FindString("autofillMetadata");
  ASSERT_TRUE(autofill_metadata);
  EXPECT_EQ(*autofill_metadata, expected_metadata);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace
}  // namespace autofill
