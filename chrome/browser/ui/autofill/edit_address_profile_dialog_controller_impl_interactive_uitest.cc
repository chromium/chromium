// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/edit_address_profile_dialog_controller_impl.h"

#include "chrome/browser/ui/views/autofill/edit_address_profile_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
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
    local_profile_ = std::make_unique<AutofillProfile>(
        AutofillProfile::RecordType::kLocalOrSyncable,
        AddressCountryCode("US"));
    local_profile_->SetRawInfoWithVerificationStatus(
        NAME_FULL, u"Mona J. Liza", VerificationStatus::kUserVerified);
    test::SetProfileInfo(local_profile_.get(), "", "", "", "email@example.com",
                         "Company Inc.", "33 Narrow Street", "Apt 42",
                         "Playa Vista", "LA", "12345", "US", "13105551234",
                         /*finalize=*/true, VerificationStatus::kUserVerified);
    local_profile_->set_language_code("en");
  }

  AutofillProfile local_profile() { return *local_profile_; }

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
          /*is_editing_existing_address*/ original_profile != nullptr,
          is_migration_to_account,
          base::BindOnce(
              &EditAddressProfileDialogControllerImplTest::OnUserDecision,
              base::Unretained(this)));
    });
  }

 private:
  // The latest user decisive interaction with the editor, e.g. Save or Cancel
  // the editor, it is set in the AddressProfileSavePromptCallback passed to the
  // prompt.
  AutofillClient::AddressPromptUserDecision user_decision_;
  std::unique_ptr<AutofillProfile> local_profile_;
  std::optional<AutofillProfile> edited_profile_;
};

IN_PROC_BROWSER_TEST_F(EditAddressProfileDialogControllerImplTest,
                       InvokeUi_LocalProfile_PressOkButton) {
  RunTestSequence(
      ShowEditor(local_profile(), nullptr, u"", false),
      // The editor popup resides in a different context on MacOS.
      InAnyContext(Steps(
          SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                                  kSuppressedScreenshotError),
          Screenshot(EditAddressProfileView::kTopViewId,
                     /*screenshot_name=*/"editor", /*baseline_cl=*/"4846629"),
          PressButton(views::DialogClientView::kOkButtonElementId),
          WaitForHide(EditAddressProfileView::kTopViewId))),
      EnsureClosedWithDecisionAndProfile(
          AutofillClient::AddressPromptUserDecision::kEditAccepted,
          local_profile()));
}

}  // namespace
}  // namespace autofill
