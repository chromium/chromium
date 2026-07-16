// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/edit_address_profile_dialog_controller_impl.h"
#include "chrome/browser/ui/views/autofill/address_editor_view.h"
#include "chrome/browser/ui/views/autofill/edit_address_profile_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/views/window/dialog_client_view.h"

namespace autofill {
namespace {

constexpr char kSuppressedScreenshotError[] =
    "Screenshot can only run in pixel_tests on Windows.";

// TODO(crbug.com/40280921): Cover EditAddressProfileDialogControllerImpl with
// more tests.
class EditAddressProfileDialogControllerImplTest
    : public InteractiveBrowserTest {
 protected:
  EditAddressProfileDialogControllerImplTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {features::kAutofillSupportSplitZipCode},
        /*disabled_features=*/{});
    local_profile_ =
        CreateTestProfile(AutofillProfile::RecordType::kLocalOrSyncable);
    account_profile_ = CreateTestProfile(AutofillProfile::RecordType::kAccount);
  }

  static std::unique_ptr<AutofillProfile> CreateTestProfile(
      AutofillProfile::RecordType record_type) {
    auto profile = std::make_unique<AutofillProfile>(record_type,
                                                     AddressCountryCode("US"));
    profile->SetRawInfoWithVerificationStatus(
        NAME_FULL, u"Mona J. Liza", VerificationStatus::kUserVerified);
    test::SetProfileInfo(profile.get(),
                         test::SetProfileInfoOptionsBuilder()
                             .with_email("email@example.com")
                             .with_company("Company Inc.")
                             .with_address1("33 Narrow Street")
                             .with_address2("Apt 42")
                             .with_city("Playa Vista")
                             .with_state("LA")
                             .with_zipcode("12345")
                             .with_country("US")
                             .with_phone("13105551234")
                             .with_status(VerificationStatus::kUserVerified)
                             .Build(),
                         /*finalize=*/true);
    profile->set_language_code("en");
    return profile;
  }

  AutofillProfile local_profile() { return *local_profile_; }

  AutofillProfile account_profile() { return *account_profile_; }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void OnUserDecision(
      AutofillClient::AddressPromptUserDecision decision,
      base::optional_ref<const AutofillProfile> edited_profile) {
    user_decision_ = decision;
    if (edited_profile.has_value()) {
      edited_profile_ = edited_profile.value();
    }
  }

  auto EnsureClosedWithDecisionAndProfile(
      AutofillClient::AddressPromptUserDecision expected_user_decision,
      base::optional_ref<const AutofillProfile> expected_profile) {
    return Steps(
        CheckResult([this]() { return user_decision_; },
                    expected_user_decision),
        Do([this, expected_profile]() {
          ASSERT_EQ(edited_profile_.has_value(), expected_profile.has_value());
          if (expected_profile.has_value()) {
            EXPECT_EQ(edited_profile_.value(), expected_profile.value());
          }
        }));
  }

  auto ShowEditor(const AutofillProfile& profile,
                  AutofillProfile* original_profile,
                  const std::u16string& footer_message,
                  bool is_migration_to_account) {
    return Do([this, profile, original_profile, footer_message,
               is_migration_to_account]() {
      user_decision_ = AutofillClient::AddressPromptUserDecision::kUndefined;

      EditAddressProfileDialogControllerImpl::CreateForWebContents(
          web_contents());
      EditAddressProfileDialogControllerImpl* const controller =
          EditAddressProfileDialogControllerImpl::FromWebContents(
              web_contents());
      ASSERT_THAT(controller, ::testing::NotNull());
      controller->OfferEdit(
          profile, /*title_override=*/u"", footer_message,
          /*is_editing_existing_address=*/original_profile != nullptr,
          is_migration_to_account,
          base::BindOnce(
              &EditAddressProfileDialogControllerImplTest::OnUserDecision,
              base::Unretained(this)));
    });
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  // The latest user decisive interaction with the editor, e.g. Save or Cancel
  // the editor, it is set in the AddressProfileSavePromptCallback passed to the
  // prompt.
  AutofillClient::AddressPromptUserDecision user_decision_;
  std::unique_ptr<AutofillProfile> local_profile_;
  std::unique_ptr<AutofillProfile> account_profile_;
  std::optional<AutofillProfile> edited_profile_;
};

IN_PROC_BROWSER_TEST_F(EditAddressProfileDialogControllerImplTest,
                       InvokeUi_LocalProfile_PressOkButton) {
  RunTestSequence(
      ShowEditor(local_profile(), nullptr, u"", false),
      // The editor popup resides in a different context on MacOS.
      InAnyContext(
          SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                                  kSuppressedScreenshotError),
          Screenshot(EditAddressProfileView::kTopViewId,
                     /*screenshot_name=*/"editor", /*baseline_cl=*/"4846629"),
          PressButton(views::DialogClientView::kOkButtonElementId),
          WaitForHide(EditAddressProfileView::kTopViewId)),
      EnsureClosedWithDecisionAndProfile(
          AutofillClient::AddressPromptUserDecision::kEditAccepted,
          local_profile()));
}

// Tests that editing an account profile enforces strict field validation (e.g.,
// clearing a required field like ZIP code disables the OK button and prevents
// accepting the dialog).
IN_PROC_BROWSER_TEST_F(EditAddressProfileDialogControllerImplTest,
                       AccountProfileStrictValidation) {
  AutofillProfile profile = account_profile();
  RunTestSequence(
      ShowEditor(profile, /*original_profile=*/nullptr, u"",
                 /*is_migration_to_account=*/false),
      InAnyContext(
          WaitForShow(EditAddressProfileView::kTopViewId),
          CheckViewProperty(views::DialogClientView::kOkButtonElementId,
                            &views::View::GetEnabled, true),
          WithView(EditAddressProfileView::kTopViewId,
                   [](views::View* view) {
                     auto* profile_view =
                         static_cast<EditAddressProfileView*>(view);
                     AddressEditorView* editor_view =
                         profile_view->GetAddressEditorViewForTesting();
                     editor_view->SetTextInputFieldValueForTesting(
                         ADDRESS_HOME_ZIP, u"");
                     EXPECT_FALSE(editor_view->ValidateAllFields());
                     EXPECT_FALSE(profile_view->Accept());
                   }),
          CheckViewProperty(views::DialogClientView::kOkButtonElementId,
                            &views::View::GetEnabled, false),
          PressButton(views::DialogClientView::kCancelButtonElementId),
          WaitForHide(EditAddressProfileView::kTopViewId)),
      EnsureClosedWithDecisionAndProfile(
          AutofillClient::AddressPromptUserDecision::kEditDeclined,
          std::nullopt));
}

// Tests that editing a local profile during account migration enforces strict
// field validation, disabling the OK button when invalid data is entered.
IN_PROC_BROWSER_TEST_F(EditAddressProfileDialogControllerImplTest,
                       LocalProfileMigratingToAccountValidation) {
  AutofillProfile profile = local_profile();
  RunTestSequence(
      ShowEditor(profile, /*original_profile=*/nullptr, u"",
                 /*is_migration_to_account=*/true),
      InAnyContext(
          WaitForShow(EditAddressProfileView::kTopViewId),
          CheckViewProperty(views::DialogClientView::kOkButtonElementId,
                            &views::View::GetEnabled, true),
          WithView(EditAddressProfileView::kTopViewId,
                   [](views::View* view) {
                     auto* profile_view =
                         static_cast<EditAddressProfileView*>(view);
                     AddressEditorView* editor_view =
                         profile_view->GetAddressEditorViewForTesting();
                     editor_view->SetTextInputFieldValueForTesting(
                         ADDRESS_HOME_ZIP, u"");
                     EXPECT_FALSE(editor_view->ValidateAllFields());
                     EXPECT_FALSE(profile_view->Accept());
                   }),
          CheckViewProperty(views::DialogClientView::kOkButtonElementId,
                            &views::View::GetEnabled, false),
          PressButton(views::DialogClientView::kCancelButtonElementId),
          WaitForHide(EditAddressProfileView::kTopViewId)),
      EnsureClosedWithDecisionAndProfile(
          AutofillClient::AddressPromptUserDecision::kEditDeclined,
          std::nullopt));
}

// Tests that editing a standard local profile (not migrating to an account)
// bypasses strict field validation, allowing invalid fields to be saved.
IN_PROC_BROWSER_TEST_F(EditAddressProfileDialogControllerImplTest,
                       LocalProfileNoMigrationValidationBypass) {
  AutofillProfile profile = local_profile();
  AutofillProfile expected_profile = profile;
  expected_profile.SetRawInfo(ADDRESS_HOME_ZIP, u"");

  RunTestSequence(
      ShowEditor(profile, /*original_profile=*/nullptr, u"",
                 /*is_migration_to_account=*/false),
      InAnyContext(
          WaitForShow(EditAddressProfileView::kTopViewId),
          CheckViewProperty(views::DialogClientView::kOkButtonElementId,
                            &views::View::GetEnabled, true),
          WithView(EditAddressProfileView::kTopViewId,
                   [](views::View* view) {
                     auto* profile_view =
                         static_cast<EditAddressProfileView*>(view);
                     AddressEditorView* editor_view =
                         profile_view->GetAddressEditorViewForTesting();
                     editor_view->SetTextInputFieldValueForTesting(
                         ADDRESS_HOME_ZIP, u"");
                     EXPECT_TRUE(editor_view->ValidateAllFields());
                   }),
          CheckViewProperty(views::DialogClientView::kOkButtonElementId,
                            &views::View::GetEnabled, true),
          PressButton(views::DialogClientView::kOkButtonElementId),
          WaitForHide(EditAddressProfileView::kTopViewId)),
      EnsureClosedWithDecisionAndProfile(
          AutofillClient::AddressPromptUserDecision::kEditAccepted,
          expected_profile));
}

}  // namespace
}  // namespace autofill
