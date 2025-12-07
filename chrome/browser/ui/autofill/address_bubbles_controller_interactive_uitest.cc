// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "build/buildflag.h"
#include "chrome/browser/ui/autofill/address_bubbles_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/autofill/edit_address_profile_view.h"
#include "chrome/browser/ui/views/autofill/save_address_profile_view.h"
#include "chrome/browser/ui/views/autofill/update_address_profile_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_test_api.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/window/dialog_client_view.h"

namespace autofill {

constexpr char kSuppressedScreenshotError[] =
    "Screenshot can only run in pixel_tests on Windows.";

constexpr char kMigrationOnTestsSuffix[] = "MigrationOn";
constexpr char kMigrationOffTestsSuffix[] = "MigrationOff";

class BaseAddressBubblesControllerTest
    : public InteractiveBrowserTest,
      public testing::WithParamInterface<bool> {
 protected:
  BaseAddressBubblesControllerTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kAutofillSupportLastNamePrefix, {}},
            {features::kAutofillSupportSplitZipCode, {}},
            {::features::kPageActionsMigration,
             {{"autofill_address", GetParam() ? "true" : "false"}}},
        },
        /*disabled_features=*/{});
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  autofill::ContentAutofillClient* autofill_client() {
    return autofill::ContentAutofillClient::FromWebContents(web_contents());
  }

  virtual void TriggerBubble() = 0;

  InteractiveTestApi::StepBuilder ShowInitBubble() {
    return Do([this]() {
      user_decision_ = AutofillClient::AddressPromptUserDecision::kUndefined;
      TriggerBubble();
    });
  }

  InteractiveTestApi::StepBuilder EnsureClosedWithDecision(
      AutofillClient::AddressPromptUserDecision expected_user_decision) {
    return Do([this, expected_user_decision]() {
      ASSERT_EQ(expected_user_decision, user_decision_);
    });
  }

  void OnUserDecision(AutofillClient::AddressPromptUserDecision decision,
                      base::optional_ref<const AutofillProfile> profile) {
    user_decision_ = decision;
  }

 private:
  // The latest user decisive interaction with a popup, e.g. Save/Update
  // or Cancel the prompt, it is set in the AddressProfileSavePromptCallback
  // passed to the prompt.
  AutofillClient::AddressPromptUserDecision user_decision_;
  base::test::ScopedFeatureList feature_list_;
};

///////////////////////////////////////////////////////////////////////////////
// SaveAddressProfileTest

class SaveAddressProfileTest : public BaseAddressBubblesControllerTest {
  void TriggerBubble() override {
    autofill_client()->ConfirmSaveAddressProfile(
        test::GetFullProfile(), nullptr,
        AutofillClient::SaveAddressBubbleType::kSave,
        base::BindOnce(&SaveAddressProfileTest::OnUserDecision,
                       base::Unretained(this)));
  }
};

IN_PROC_BROWSER_TEST_P(SaveAddressProfileTest, SaveAccept) {
  RunTestSequence(ShowInitBubble(),
                  PressButton(views::DialogClientView::kOkButtonElementId),
                  EnsureClosedWithDecision(
                      AutofillClient::AddressPromptUserDecision::kAccepted));
}

IN_PROC_BROWSER_TEST_P(SaveAddressProfileTest, SaveDecline) {
  RunTestSequence(ShowInitBubble(),
                  PressButton(views::DialogClientView::kCancelButtonElementId),
                  EnsureClosedWithDecision(
                      AutofillClient::AddressPromptUserDecision::kDeclined));
}

IN_PROC_BROWSER_TEST_P(SaveAddressProfileTest, SaveWithEdit) {
  RunTestSequence(
      ShowInitBubble(),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              kSuppressedScreenshotError),
      Screenshot(SaveAddressProfileView::kTopViewId,
                 /*screenshot_name=*/"save_popup", /*baseline_cl=*/"4535916"),
      PressButton(SaveAddressProfileView::kEditButtonViewId),

      WaitForShow(EditAddressProfileView::kTopViewId),
      Screenshot(EditAddressProfileView::kTopViewId,
                 /*screenshot_name=*/"edit_popup",
                 /*baseline_cl=*/"4535916"),
      PressButton(views::DialogClientView::kCancelButtonElementId),
      WaitForHide(EditAddressProfileView::kTopViewId),

      WaitForShow(SaveAddressProfileView::kTopViewId),
      PressButton(views::DialogClientView::kOkButtonElementId),
      WaitForHide(SaveAddressProfileView::kTopViewId),
      EnsureClosedWithDecision(
          AutofillClient::AddressPromptUserDecision::kAccepted));
}

IN_PROC_BROWSER_TEST_P(SaveAddressProfileTest, SaveInEdit) {
  RunTestSequence(
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              kSuppressedScreenshotError),
      ShowInitBubble(), PressButton(SaveAddressProfileView::kEditButtonViewId),

      WaitForShow(EditAddressProfileView::kTopViewId),
      PressButton(views::DialogClientView::kOkButtonElementId),
      WaitForHide(EditAddressProfileView::kTopViewId),

      EnsureClosedWithDecision(
          AutofillClient::AddressPromptUserDecision::kEditAccepted));
}

IN_PROC_BROWSER_TEST_P(SaveAddressProfileTest, SaveCloseAndOpenAgain) {
  RunTestSequence(
      ShowInitBubble(),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              kSuppressedScreenshotError),
      Screenshot(SaveAddressProfileView::kTopViewId,
                 /*screenshot_name=*/"save_popup", /*baseline_cl=*/"4535916"),

      PressButton(views::BubbleFrameView::kCloseButtonElementId),
      // Make sure the popup gets closed before subsequent reopening.
      EnsureNotPresent(SaveAddressProfileView::kTopViewId),

      ShowInitBubble(),
      Screenshot(SaveAddressProfileView::kTopViewId,
                 /*screenshot_name=*/"reopened_save_popup",
                 /*baseline_cl=*/"4535916"));
}

IN_PROC_BROWSER_TEST_P(SaveAddressProfileTest, NoCrashesOnTabClose) {
  RunTestSequence(
      ShowInitBubble(), EnsurePresent(SaveAddressProfileView::kTopViewId),
      Do([this]() {
        browser()->tab_strip_model()->GetActiveWebContents()->Close();
      }));
}

///////////////////////////////////////////////////////////////////////////////
// UpdateAddressProfileTest

class UpdateAddressProfileTest : public BaseAddressBubblesControllerTest {
 protected:
  void TriggerBubble() override {
    autofill_client()->ConfirmSaveAddressProfile(
        test::GetFullProfile(), &original_profile_,
        AutofillClient::SaveAddressBubbleType::kSave,
        base::BindOnce(&UpdateAddressProfileTest::OnUserDecision,
                       base::Unretained(this)));
  }

  AutofillProfile original_profile_ = test::GetFullProfile2();
};

IN_PROC_BROWSER_TEST_P(UpdateAddressProfileTest, UpdateThroughEdit) {
  RunTestSequence(
      ShowInitBubble(),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              kSuppressedScreenshotError),
      Screenshot(UpdateAddressProfileView::kTopViewId,
                 /*screenshot_name=*/"update_popup",
                 /*baseline_cl=*/"4535916"),
      PressButton(UpdateAddressProfileView::kEditButtonViewId),

      WaitForShow(EditAddressProfileView::kTopViewId),
      Screenshot(EditAddressProfileView::kTopViewId,
                 /*screenshot_name=*/"edit_popup",
                 /*baseline_cl=*/"4535916"),
      PressButton(views::DialogClientView::kCancelButtonElementId),
      WaitForHide(EditAddressProfileView::kTopViewId),

      WaitForShow(UpdateAddressProfileView::kTopViewId),
      PressButton(views::DialogClientView::kOkButtonElementId),
      WaitForHide(UpdateAddressProfileView::kTopViewId),
      EnsureClosedWithDecision(
          AutofillClient::AddressPromptUserDecision::kAccepted));
}

///////////////////////////////////////////////////////////////////////////////
// UpdateAccountAddressProfileTest

class UpdateAccountAddressProfileTest : public UpdateAddressProfileTest {
  void TriggerBubble() override {
    test_api(original_profile_)
        .set_record_type(AutofillProfile::RecordType::kAccount);
    autofill_client()->ConfirmSaveAddressProfile(
        test::GetFullProfile(), &original_profile_,
        AutofillClient::SaveAddressBubbleType::kSave,
        base::BindOnce(&UpdateAccountAddressProfileTest::OnUserDecision,
                       base::Unretained(this)));
  }
};

IN_PROC_BROWSER_TEST_P(UpdateAccountAddressProfileTest, UpdateThroughEdit) {
  RunTestSequence(
      ShowInitBubble(),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              kSuppressedScreenshotError),
      Screenshot(UpdateAddressProfileView::kTopViewId,
                 /*screenshot_name=*/"update_popup",
                 /*baseline_cl=*/"4535916"),
      PressButton(UpdateAddressProfileView::kEditButtonViewId),

      WaitForShow(EditAddressProfileView::kTopViewId),
      Screenshot(EditAddressProfileView::kTopViewId,
                 /*screenshot_name=*/"edit_popup",
                 /*baseline_cl=*/"4535916"),
      PressButton(views::DialogClientView::kCancelButtonElementId),
      WaitForHide(EditAddressProfileView::kTopViewId),

      WaitForShow(UpdateAddressProfileView::kTopViewId),
      PressButton(views::DialogClientView::kOkButtonElementId),
      WaitForHide(UpdateAddressProfileView::kTopViewId),
      EnsureClosedWithDecision(
          AutofillClient::AddressPromptUserDecision::kAccepted));
}

///////////////////////////////////////////////////////////////////////////////
// SaveAddressProfileTest

class MigrateToProfileAddressProfileTest
    : public BaseAddressBubblesControllerTest {
  void TriggerBubble() override {
    autofill_client()->ConfirmSaveAddressProfile(
        test::GetFullProfile(), nullptr,
        AutofillClient::SaveAddressBubbleType::kMigrateToAccount,
        base::BindOnce(&MigrateToProfileAddressProfileTest::OnUserDecision,
                       base::Unretained(this)));
  }
};

IN_PROC_BROWSER_TEST_P(MigrateToProfileAddressProfileTest, SaveDecline) {
  RunTestSequence(ShowInitBubble(),
                  PressButton(views::DialogClientView::kCancelButtonElementId),
                  EnsureClosedWithDecision(
                      AutofillClient::AddressPromptUserDecision::kNever));
}

IN_PROC_BROWSER_TEST_P(MigrateToProfileAddressProfileTest, SaveWithEdit) {
  RunTestSequence(
      ShowInitBubble(),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              kSuppressedScreenshotError),
      Screenshot(SaveAddressProfileView::kTopViewId, "save_popup", "4535916"),
      PressButton(SaveAddressProfileView::kEditButtonViewId),

      WaitForShow(EditAddressProfileView::kTopViewId),
      Screenshot(EditAddressProfileView::kTopViewId,
                 /*screenshot_name=*/"edit_popup",
                 /*baseline_cl=*/"4535916"),
      PressButton(views::DialogClientView::kCancelButtonElementId),
      WaitForHide(EditAddressProfileView::kTopViewId),

      WaitForShow(SaveAddressProfileView::kTopViewId),
      PressButton(views::DialogClientView::kOkButtonElementId),
      WaitForHide(SaveAddressProfileView::kTopViewId),
      EnsureClosedWithDecision(
          AutofillClient::AddressPromptUserDecision::kAccepted));
}

// TODO(crbug.com/356845298): Add a test for combining the `kAccountNameEmail`
// profile with one of the `kAccountHome`/`kAccountWork` profiles.

INSTANTIATE_TEST_SUITE_P(AllAutofillAddressStates,
                         SaveAddressProfileTest,
                         ::testing::Bool(),
                         [](auto const& info) {
                           return info.param ? kMigrationOnTestsSuffix
                                             : kMigrationOffTestsSuffix;
                         });

INSTANTIATE_TEST_SUITE_P(AllAutofillAddressStates,
                         UpdateAddressProfileTest,
                         ::testing::Bool(),
                         [](auto const& info) {
                           return info.param ? kMigrationOnTestsSuffix
                                             : kMigrationOffTestsSuffix;
                         });

INSTANTIATE_TEST_SUITE_P(AllAutofillAddressStates,
                         UpdateAccountAddressProfileTest,
                         ::testing::Bool(),
                         [](auto const& info) {
                           return info.param ? kMigrationOnTestsSuffix
                                             : kMigrationOffTestsSuffix;
                         });

INSTANTIATE_TEST_SUITE_P(AllAutofillAddressStates,
                         MigrateToProfileAddressProfileTest,
                         ::testing::Bool(),
                         [](auto const& info) {
                           return info.param ? kMigrationOnTestsSuffix
                                             : kMigrationOffTestsSuffix;
                         });

}  // namespace autofill
