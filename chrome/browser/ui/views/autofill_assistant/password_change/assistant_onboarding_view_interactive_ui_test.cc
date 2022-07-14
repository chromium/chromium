// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill_assistant/password_change/assistant_onboarding_view.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/autofill_assistant/password_change/apc_onboarding_coordinator.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_prompt.h"
#include "chrome/browser/ui/autofill_assistant/password_change/mock_assistant_onboarding_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "url/gurl.h"

// Simple test fixture for testing `AssistantOnboardingView` that checks
// whether accepting/cancelling the dialog works and whether the labels
// contains the text specified in the `AssistantOnboardingInformation` model.
class AssistantOnboardingViewBrowserTest : public DialogBrowserTest {
 public:
  AssistantOnboardingViewBrowserTest() = default;
  ~AssistantOnboardingViewBrowserTest() override = default;

  // Creates a model with the same data that is used for the automated password
  // change onboarding dialog.
  void UseApcModel() {
    model_ = ApcOnboardingCoordinator::CreateOnboardingInformation();
  }

  // Creates the controller and view and calls their `Show()` method.
  void ShowUi(const std::string& name) override {
    // Pick the correct model for the dialog.
    if (name == "Apc") {
      UseApcModel();
    } else {
      NOTREACHED();
    }

    controller_ = AssistantOnboardingController::Create(
        model_, browser()->tab_strip_model()->GetActiveWebContents());
    // We do not use the factory function here to test `AssistantOnboardingView`
    // directly.
    view_ = new AssistantOnboardingView(controller()->GetWeakPtr());
    controller()->Show(prompt(), callback().Get());
  }

  // Verifies that the UI is working correctly.
  bool VerifyUi() override {
    if (!DialogBrowserTest::VerifyUi())
      return false;

    if (GetTextFromLabel(AssistantOnboardingView::DialogViewID::TITLE) !=
        l10n_util::GetStringUTF16(model()->title_id)) {
      return false;
    }

    if (GetTextFromLabel(AssistantOnboardingView::DialogViewID::DESCRIPTION) !=
        l10n_util::GetStringUTF16(model()->description_id)) {
      return false;
    }

    if (GetTextFromStyledLabel(
            AssistantOnboardingView::DialogViewID::CONSENT_TEXT) !=
        l10n_util::GetStringFUTF16(
            model()->consent_text_id,
            l10n_util::GetStringUTF16(model()->learn_more_title_id))) {
      return false;
    }

    return true;
  }

  // Returns the text from a label with element ID `view_id`. The element
  // must be a `views::Label`.
  std::u16string GetTextFromLabel(
      AssistantOnboardingView::DialogViewID view_id) {
    return static_cast<views::Label*>(
               view()->GetViewByID(static_cast<int>(view_id)))
        ->GetText();
  }

  // Returns the text from a styled label with element ID `view_id`. The element
  // must be a views::StyledLabel`.
  std::u16string GetTextFromStyledLabel(
      AssistantOnboardingView::DialogViewID view_id) {
    return static_cast<views::StyledLabel*>(
               view()->GetViewByID(static_cast<int>(view_id)))
        ->GetText();
  }

  // Getter methods for private members.
  AssistantOnboardingView* view() { return view_; }
  base::WeakPtr<AssistantOnboardingPrompt> prompt() {
    return view_->GetWeakPtr();
  }
  AssistantOnboardingController* controller() { return controller_.get(); }
  AssistantOnboardingInformation* model() { return &model_; }
  base::MockCallback<AssistantOnboardingController::Callback>& callback() {
    return callback_;
  }

 private:
  // Test support.
  AssistantOnboardingInformation model_;
  std::unique_ptr<AssistantOnboardingController> controller_;
  base::MockCallback<AssistantOnboardingController::Callback> callback_;

  // The object to be tested.
  raw_ptr<AssistantOnboardingView> view_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(AssistantOnboardingViewBrowserTest, CancelDialog) {
  ShowUi("Apc");

  // We expect the controller to signal back that the dialog was cancelled.
  EXPECT_CALL(callback(),
              Run(false, /*confirmation_grd_id=*/absl::optional<int>(),
                  /*description_grd_ids=*/std::vector<int>()));
  view()->CancelDialog();
}

IN_PROC_BROWSER_TEST_F(AssistantOnboardingViewBrowserTest, AcceptDialog) {
  ShowUi("Apc");

  // We expect the controller to signal back that the dialog was accepted.
  const std::vector<int> description_ids = {
      model()->title_id, model()->description_id, model()->consent_text_id,
      model()->learn_more_title_id};
  EXPECT_CALL(callback(),
              Run(true, absl::optional<int>(model()->button_accept_text_id),
                  description_ids));
  view()->AcceptDialog();
}

IN_PROC_BROWSER_TEST_F(AssistantOnboardingViewBrowserTest, InvokeUi_Apc) {
  ShowAndVerifyUi();
}
