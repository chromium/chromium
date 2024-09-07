// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_save_update_address_profile_delegate_ios.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile_test_api.h"
#include "components/autofill/ios/common/features.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {

constexpr char16_t kTestEmail[] = u"test@email.com";
using ::testing::Property;
using profile_ref = base::optional_ref<const AutofillProfile>;
constexpr int kNavEntryId = 10;

class AutofillSaveUpdateAddressProfileDelegateIOSTest : public testing::Test {
 protected:
  AutofillSaveUpdateAddressProfileDelegateIOSTest() = default;
  ~AutofillSaveUpdateAddressProfileDelegateIOSTest() override {}

  void SetUp() override {
    delegate_ = CreateAutofillSaveUpdateAddressProfileDelegate();
  }

  std::unique_ptr<AutofillSaveUpdateAddressProfileDelegateIOS>
  CreateAutofillSaveUpdateAddressProfileDelegate(
      AutofillProfile* original_profile = nullptr,
      std::optional<std::u16string> email = std::nullopt,
      bool is_migration_to_account = false,
      bool is_account_profile = false) {
    profile_ = std::make_unique<AutofillProfile>(test::GetFullProfile());
    if (is_account_profile) {
      test_api(*profile_).set_record_type(
          autofill::AutofillProfile::RecordType::kAccount);
    }
    return std::make_unique<AutofillSaveUpdateAddressProfileDelegateIOS>(
        *profile_, original_profile, email,
        /*locale=*/"en-US", is_migration_to_account, callback_.Get());
  }

  std::unique_ptr<AutofillProfile> profile_;
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback>
      callback_;
  infobars::InfoBarDelegate::NavigationDetails nav_details_that_expire_{
      .entry_id = kNavEntryId,
      .is_navigation_to_different_page = true,
      .did_replace_entry = false,
      .is_reload = true,
      .is_redirect = false,
      .is_form_submission = false,
      .has_user_gesture = true};
  std::unique_ptr<AutofillSaveUpdateAddressProfileDelegateIOS> delegate_ =
      CreateAutofillSaveUpdateAddressProfileDelegate();
};

// Tests that the callback is run with kAccepted on Accepted.
TEST_F(AutofillSaveUpdateAddressProfileDelegateIOSTest,
       HandleUserAction_Accepted) {
  EXPECT_CALL(callback_,
              Run(AutofillClient::AddressPromptUserDecision::kAccepted,
                  Property(&profile_ref::has_value, false)));
  delegate_->Accept();
}

// Tests that the callback is run with kDeclined on destruction.
TEST_F(AutofillSaveUpdateAddressProfileDelegateIOSTest,
       TestCallbackOnDestruction) {
  delegate_->Cancel();
  EXPECT_CALL(callback_,
              Run(AutofillClient::AddressPromptUserDecision::kDeclined,
                  Property(&profile_ref::has_value, false)));
  // The callback should run in the destructor.
  delegate_.reset();
}

// Tests that the callback is run with kAccepted on Accept.
TEST_F(AutofillSaveUpdateAddressProfileDelegateIOSTest, TestCallbackOnSave) {
  EXPECT_CALL(callback_,
              Run(AutofillClient::AddressPromptUserDecision::kAccepted,
                  Property(&profile_ref::has_value, false)));
  delegate_->Accept();
}

// Tests that the callback is run with kEditAccepted on EditAccepted.
TEST_F(AutofillSaveUpdateAddressProfileDelegateIOSTest,
       TestCallbackOnEditAccepted) {
  EXPECT_CALL(callback_,
              Run(AutofillClient::AddressPromptUserDecision::kEditAccepted,
                  testing::_));
  delegate_->EditAccepted();
}

// Tests that the callback is run with kNever on Never.
TEST_F(AutofillSaveUpdateAddressProfileDelegateIOSTest,
       HandleUserAction_NoThanks) {
  EXPECT_CALL(callback_, Run(AutofillClient::AddressPromptUserDecision::kNever,
                             Property(&profile_ref::has_value, false)));
  delegate_->Never();
}

// Tests that the infobar expires when reloading the page.
TEST_F(AutofillSaveUpdateAddressProfileDelegateIOSTest,
       ShouldExpire_True_WhenReload) {
  nav_details_that_expire_.is_reload = true;
  nav_details_that_expire_.entry_id = kNavEntryId;
  delegate_->set_nav_entry_id(kNavEntryId);
  EXPECT_TRUE(delegate_->ShouldExpire(nav_details_that_expire_));
}

// Tests that the infobar expires when new navigation ID.
TEST_F(AutofillSaveUpdateAddressProfileDelegateIOSTest,
       ShouldExpire_True_WhenDifferentNavEntryId) {
  nav_details_that_expire_.is_reload = false;
  nav_details_that_expire_.entry_id = kNavEntryId;
  const int different_nav_id = kNavEntryId - 1;
  delegate_->set_nav_entry_id(different_nav_id);

  EXPECT_TRUE(delegate_->ShouldExpire(nav_details_that_expire_));
}

// Tests that when the sticky infobar is disabled, having a user gesture isn't
// used as a condition to expire the infobar, hence setting the user gesture bit
// to false shouldn't change the returned value.
TEST_F(AutofillSaveUpdateAddressProfileDelegateIOSTest,
       ShouldExpire_True_WhenNoStickyInfobarAndNoUserGesture) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kAutofillStickyInfobarIos);

  nav_details_that_expire_.has_user_gesture = false;

  EXPECT_TRUE(delegate_->ShouldExpire(nav_details_that_expire_));
}

// Tests that when the sticky infobar is enabled, having a user gesture is
// used as a condition to expire the infobar, hence setting the user gesture bit
// to true should return true.
TEST_F(AutofillSaveUpdateAddressProfileDelegateIOSTest,
       ShouldExpire_True_WhenStickyInfobarAndUserGesture) {
  nav_details_that_expire_.has_user_gesture = true;
  EXPECT_TRUE(delegate_->ShouldExpire(nav_details_that_expire_));
}

// Tests that the infobar doesn't expire when the page is the same.
TEST_F(AutofillSaveUpdateAddressProfileDelegateIOSTest,
       ShouldExpire_False_WhenNoDifferentPage) {
  nav_details_that_expire_.is_navigation_to_different_page = false;
  EXPECT_FALSE(delegate_->ShouldExpire(nav_details_that_expire_));
}

// Tests that the infobar doesn't expire when only the history entry is
// replaced.
TEST_F(AutofillSaveUpdateAddressProfileDelegateIOSTest,
       ShouldExpire_False_WhenDidReplaceEntry) {
  nav_details_that_expire_.did_replace_entry = true;
  EXPECT_FALSE(delegate_->ShouldExpire(nav_details_that_expire_));
}

// Tests that the infobar doesn't expire when redirect.
TEST_F(AutofillSaveUpdateAddressProfileDelegateIOSTest,
       ShouldExpire_False_WhenRedirect) {
  nav_details_that_expire_.is_redirect = true;
  EXPECT_FALSE(delegate_->ShouldExpire(nav_details_that_expire_));
}

// Tests that the infobar doesn't expire when form submission.
TEST_F(AutofillSaveUpdateAddressProfileDelegateIOSTest,
       ShouldExpire_False_WhenFormSubmission) {
  nav_details_that_expire_.is_form_submission = true;
  EXPECT_FALSE(delegate_->ShouldExpire(nav_details_that_expire_));
}

// Tests that the infobar expires when no reload and the navigation entry ID
// didn't change.
TEST_F(AutofillSaveUpdateAddressProfileDelegateIOSTest,
       ShouldExpire_False_WhenNoReloadAndSameNavEntryId) {
  nav_details_that_expire_.is_reload = false;
  nav_details_that_expire_.entry_id = kNavEntryId;
  delegate_->set_nav_entry_id(kNavEntryId);
  EXPECT_FALSE(delegate_->ShouldExpire(nav_details_that_expire_));
}

// Tests that when the sticky infobar is enabled, having a user gesture is
// used as a condition to expire the infobar, hence setting the user gesture bit
// to false should return false.
TEST_F(AutofillSaveUpdateAddressProfileDelegateIOSTest,
       ShouldExpire_False_WhenStickyInfobar) {
  nav_details_that_expire_.has_user_gesture = false;
  EXPECT_FALSE(delegate_->ShouldExpire(nav_details_that_expire_));
}

struct DelegateStringsTestCase {
  bool is_migration;
  bool is_update;
  bool is_account_profile;
  int expected_message_action_text_id;
  int expected_message_text_id;
  absl::variant<int, std::u16string> expected_description_or_id;
};

class DelegateStringsTest
    : public AutofillSaveUpdateAddressProfileDelegateIOSTest,
      public ::testing::WithParamInterface<DelegateStringsTestCase> {
 protected:
  bool is_migration() const { return GetParam().is_migration; }
  bool is_update() const { return GetParam().is_update; }
  bool is_account_profile() const { return GetParam().is_account_profile; }
};

// Tests the message title, subtitle and action text strings.
TEST_P(DelegateStringsTest, TestStrings) {
  AutofillProfile original_profile = test::GetFullProfile();
  original_profile.SetInfo(NAME_FULL, u"John Doe", "en-US");

  delegate_ = CreateAutofillSaveUpdateAddressProfileDelegate(
      is_update() ? &original_profile : nullptr, kTestEmail, is_migration(),
      is_account_profile());

  const DelegateStringsTestCase& test_case = GetParam();
  EXPECT_EQ(
      delegate_->GetMessageActionText(),
      l10n_util::GetStringUTF16(test_case.expected_message_action_text_id));
  EXPECT_EQ(delegate_->GetMessageText(),
            l10n_util::GetStringUTF16(test_case.expected_message_text_id));
  if (absl::holds_alternative<int>(test_case.expected_description_or_id)) {
    EXPECT_EQ(
        delegate_->GetDescription(),
        l10n_util::GetStringFUTF16(
            absl::get<int>(test_case.expected_description_or_id), kTestEmail));
  } else {
    EXPECT_EQ(delegate_->GetDescription(),
              absl::get<std::u16string>(test_case.expected_description_or_id));
  }
}

INSTANTIATE_TEST_SUITE_P(
    AutofillSaveUpdateAddressProfileDelegateIOSTest,
    DelegateStringsTest,
    testing::Values(
        // Tests strings for the save profile views.
        DelegateStringsTestCase{
            false, false, false,
            IDS_IOS_AUTOFILL_SAVE_ADDRESS_MESSAGE_PRIMARY_ACTION,
            IDS_IOS_AUTOFILL_SAVE_ADDRESS_MESSAGE_TITLE,
            u"John H. Doe, 666 Erebus St."},
        // Tests strings for the save profile in Google Account views.
        DelegateStringsTestCase{
            false, false, true,
            IDS_IOS_AUTOFILL_SAVE_ADDRESS_MESSAGE_PRIMARY_ACTION,
            IDS_IOS_AUTOFILL_SAVE_ADDRESS_MESSAGE_TITLE,
            IDS_IOS_AUTOFILL_SAVE_ADDRESS_IN_ACCOUNT_MESSAGE_SUBTITLE},
        // Test strings for the migration view.
        DelegateStringsTestCase{
            true, false, false,
            IDS_IOS_AUTOFILL_SAVE_ADDRESS_MESSAGE_PRIMARY_ACTION,
            IDS_IOS_AUTOFILL_SAVE_ADDRESS_IN_ACCOUNT_MESSAGE_TITLE,
            u"You can use it across Google products"},
        // Test strings for the update views.
        DelegateStringsTestCase{
            false, true, false,
            IDS_IOS_AUTOFILL_UPDATE_ADDRESS_MESSAGE_PRIMARY_ACTION,
            IDS_IOS_AUTOFILL_UPDATE_ADDRESS_MESSAGE_TITLE,
            u"John Doe, 666 Erebus St."},
        // Test strings for the update in Google Account views.
        DelegateStringsTestCase{
            false, true, true,
            IDS_IOS_AUTOFILL_UPDATE_ADDRESS_MESSAGE_PRIMARY_ACTION,
            IDS_IOS_AUTOFILL_UPDATE_ADDRESS_MESSAGE_TITLE,
            u"John Doe, 666 Erebus St."}));

}  // namespace
}  // namespace autofill
