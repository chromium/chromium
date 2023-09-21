// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/save_update_address_profile_bubble_controller_impl.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/views/autofill/edit_address_profile_view.h"
#include "chrome/browser/ui/views/autofill/save_address_profile_view.h"
#include "chrome/browser/ui/views/autofill/update_address_profile_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/window/dialog_client_view.h"

namespace autofill {

constexpr char kSuppressedScreenshotError[] =
    "Screenshot can only run in pixel_tests on Windows.";

class BaseSaveUpdateAddressProfileBubbleControllerImplTest
    : public InteractiveBrowserTest {
 protected:
  autofill::ContentAutofillClient* autofill_client() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return autofill::ContentAutofillClient::FromWebContents(web_contents);
  }

  virtual void TriggerBubble() = 0;

  InteractiveTestApi::StepBuilder ShowInitBubble() {
    return Do([this]() {
      user_decision_ =
          AutofillClient::SaveAddressProfileOfferUserDecision::kUndefined;
      TriggerBubble();
    });
  }

  InteractiveTestApi::StepBuilder EnsureClosedWithDecision(
      AutofillClient::SaveAddressProfileOfferUserDecision
          expected_user_decision) {
    return Do([this, expected_user_decision]() {
      ASSERT_EQ(expected_user_decision, user_decision_);
    });
  }

  void OnUserDecision(
      AutofillClient::SaveAddressProfileOfferUserDecision decision,
      AutofillProfile profile) {
    user_decision_ = decision;
  }

 private:
  // The latest user decisive interaction with a popup, e.g. Save/Update
  // or Cancel the prompt, it is set in the AddressProfileSavePromptCallback
  // passed to the prompt.
  AutofillClient::SaveAddressProfileOfferUserDecision user_decision_;
};

///////////////////////////////////////////////////////////////////////////////
// SaveAddressProfileTest

class SaveAddressProfileTest
    : public BaseSaveUpdateAddressProfileBubbleControllerImplTest {
  void TriggerBubble() override {
    autofill_client()->ConfirmSaveAddressProfile(
        test::GetFullProfile(), nullptr,
        AutofillClient::SaveAddressProfilePromptOptions{.show_prompt = true},
        base::BindOnce(&SaveAddressProfileTest::OnUserDecision,
                       base::Unretained(this)));
  }
};

IN_PROC_BROWSER_TEST_F(SaveAddressProfileTest, SaveAccept) {
  RunTestSequence(
      ShowInitBubble(),
      PressButton(views::DialogClientView::kOkButtonElementId),
      EnsureClosedWithDecision(
          AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted));
}

IN_PROC_BROWSER_TEST_F(SaveAddressProfileTest, SaveDecline) {
  RunTestSequence(
      ShowInitBubble(),
      PressButton(views::DialogClientView::kCancelButtonElementId),
      EnsureClosedWithDecision(
          AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined));
}

IN_PROC_BROWSER_TEST_F(SaveAddressProfileTest, SaveWithEdit) {
  RunTestSequence(
      ShowInitBubble(),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              kSuppressedScreenshotError),
      Screenshot(SaveAddressProfileView::kTopViewId, "save_popup", "4535916"),
      PressButton(SaveAddressProfileView::kEditButtonViewId),

      // The editor popup resides in a different context on MacOS.
      InAnyContext(
          Steps(WaitForShow(EditAddressProfileView::kTopViewId),
                Screenshot(EditAddressProfileView::kTopViewId, "edit_popup",
                           "4535916"),
                PressButton(views::DialogClientView::kCancelButtonElementId))),

      WaitForShow(SaveAddressProfileView::kTopViewId),
      PressButton(views::DialogClientView::kOkButtonElementId),
      WaitForHide(SaveAddressProfileView::kTopViewId),
      EnsureClosedWithDecision(
          AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted));
}

IN_PROC_BROWSER_TEST_F(SaveAddressProfileTest, SaveInEdit) {
  RunTestSequence(
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              kSuppressedScreenshotError),
      ShowInitBubble(), PressButton(SaveAddressProfileView::kEditButtonViewId),

      InAnyContext(Steps(
          WaitForShow(EditAddressProfileView::kTopViewId),
          PressButton(views::DialogClientView::kOkButtonElementId),
          WaitForHide(EditAddressProfileView::kTopViewId), FlushEvents())),

      EnsureClosedWithDecision(
          AutofillClient::SaveAddressProfileOfferUserDecision::kEditAccepted));
}

IN_PROC_BROWSER_TEST_F(SaveAddressProfileTest, SaveCloseAndOpenAgain) {
  RunTestSequence(
      ShowInitBubble(),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              kSuppressedScreenshotError),
      Screenshot(SaveAddressProfileView::kTopViewId, "save_popup", "4535916"),

      PressButton(views::BubbleFrameView::kCloseButtonElementId),
      // Make sure the popup gets closed before subsequent reopening.
      EnsureNotPresent(SaveAddressProfileView::kTopViewId),

      ShowInitBubble(),
      Screenshot(SaveAddressProfileView::kTopViewId, "reopened_save_popup",
                 "4535916"));
}

IN_PROC_BROWSER_TEST_F(SaveAddressProfileTest, NoCrashesOnTabClose) {
  RunTestSequence(
      ShowInitBubble(), EnsurePresent(SaveAddressProfileView::kTopViewId),
      Do([this]() {
        browser()->tab_strip_model()->GetActiveWebContents()->Close();
      }));
}

///////////////////////////////////////////////////////////////////////////////
// UpdateAddressProfileTest

class UpdateAddressProfileTest
    : public BaseSaveUpdateAddressProfileBubbleControllerImplTest {
 protected:
  void TriggerBubble() override {
    autofill_client()->ConfirmSaveAddressProfile(
        test::GetFullProfile(), &original_profile_,
        AutofillClient::SaveAddressProfilePromptOptions{.show_prompt = true},
        base::BindOnce(&UpdateAddressProfileTest::OnUserDecision,
                       base::Unretained(this)));
  }

  AutofillProfile original_profile_ = test::GetFullProfile2();
};

IN_PROC_BROWSER_TEST_F(UpdateAddressProfileTest, UpdateThroughEdit) {
  RunTestSequence(
      ShowInitBubble(),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              kSuppressedScreenshotError),
      Screenshot(UpdateAddressProfileView::kTopViewId, "update_popup",
                 "4535916"),
      PressButton(UpdateAddressProfileView::kEditButtonViewId),

      // The editor popup resides in a different context on MacOS.
      InAnyContext(
          Steps(WaitForShow(EditAddressProfileView::kTopViewId),
                Screenshot(EditAddressProfileView::kTopViewId, "edit_popup",
                           "4535916"),
                PressButton(views::DialogClientView::kCancelButtonElementId))),

      WaitForShow(UpdateAddressProfileView::kTopViewId),
      PressButton(views::DialogClientView::kOkButtonElementId),
      WaitForHide(UpdateAddressProfileView::kTopViewId),
      EnsureClosedWithDecision(
          AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted));
}

///////////////////////////////////////////////////////////////////////////////
// UpdateAccountAddressProfileTest

class UpdateAccountAddressProfileTest : public UpdateAddressProfileTest {
  void TriggerBubble() override {
    original_profile_.set_source_for_testing(AutofillProfile::Source::kAccount);
    autofill_client()->ConfirmSaveAddressProfile(
        test::GetFullProfile(), &original_profile_,
        AutofillClient::SaveAddressProfilePromptOptions{.show_prompt = true},
        base::BindOnce(&UpdateAccountAddressProfileTest::OnUserDecision,
                       base::Unretained(this)));
  }
};

IN_PROC_BROWSER_TEST_F(UpdateAccountAddressProfileTest, UpdateThroughEdit) {
  RunTestSequence(
      ShowInitBubble(),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              kSuppressedScreenshotError),
      Screenshot(UpdateAddressProfileView::kTopViewId, "update_popup",
                 "4535916"),
      PressButton(UpdateAddressProfileView::kEditButtonViewId),

      // The editor popup resides in a different context on MacOS.
      InAnyContext(
          Steps(WaitForShow(EditAddressProfileView::kTopViewId),
                Screenshot(EditAddressProfileView::kTopViewId, "edit_popup",
                           "4535916"),
                PressButton(views::DialogClientView::kCancelButtonElementId))),

      WaitForShow(UpdateAddressProfileView::kTopViewId),
      PressButton(views::DialogClientView::kOkButtonElementId),
      WaitForHide(UpdateAddressProfileView::kTopViewId),
      EnsureClosedWithDecision(
          AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted));
}

///////////////////////////////////////////////////////////////////////////////
// SaveAddressProfileTest

class MigrateToProfileAddressProfileTest
    : public BaseSaveUpdateAddressProfileBubbleControllerImplTest {
  void TriggerBubble() override {
    autofill_client()->ConfirmSaveAddressProfile(
        test::GetFullProfile(), nullptr,
        AutofillClient::SaveAddressProfilePromptOptions{
            .show_prompt = true, .is_migration_to_account = true},
        base::BindOnce(&MigrateToProfileAddressProfileTest::OnUserDecision,
                       base::Unretained(this)));
  }
};

IN_PROC_BROWSER_TEST_F(MigrateToProfileAddressProfileTest, SaveDecline) {
  RunTestSequence(
      ShowInitBubble(),
      PressButton(views::DialogClientView::kCancelButtonElementId),
      EnsureClosedWithDecision(
          AutofillClient::SaveAddressProfileOfferUserDecision::kNever));
}

IN_PROC_BROWSER_TEST_F(MigrateToProfileAddressProfileTest, SaveWithEdit) {
  RunTestSequence(
      ShowInitBubble(),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              kSuppressedScreenshotError),
      Screenshot(SaveAddressProfileView::kTopViewId, "save_popup", "4535916"),
      PressButton(SaveAddressProfileView::kEditButtonViewId),

      // The editor popup resides in a different context on MacOS.
      InAnyContext(
          Steps(WaitForShow(EditAddressProfileView::kTopViewId),
                Screenshot(EditAddressProfileView::kTopViewId, "edit_popup",
                           "4535916"),
                PressButton(views::DialogClientView::kCancelButtonElementId))),

      WaitForShow(SaveAddressProfileView::kTopViewId),
      PressButton(views::DialogClientView::kOkButtonElementId),
      WaitForHide(SaveAddressProfileView::kTopViewId),
      EnsureClosedWithDecision(
          AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted));
}

}  // namespace autofill
