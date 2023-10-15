// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_save_update_address_profile_delegate_ios.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

constexpr char16_t kTestEmail[] = u"test@email.com";
using ::testing::Property;
using profile_ref = base::optional_ref<const AutofillProfile>;

}  // namespace

class AutofillSaveUpdateAddressProfileDelegateIOSTest : public testing::Test {
 protected:
  AutofillSaveUpdateAddressProfileDelegateIOSTest() = default;
  ~AutofillSaveUpdateAddressProfileDelegateIOSTest() override {}

  std::unique_ptr<AutofillSaveUpdateAddressProfileDelegateIOS>
  CreateAutofillSaveUpdateAddressProfileDelegate(
      AutofillProfile* original_profile = nullptr,
      absl::optional<std::u16string> email = absl::nullopt,
      bool is_migration_to_account = false,
      bool is_account_profile = false) {
    profile_ = test::GetFullProfile();
    if (is_account_profile) {
      profile_.set_source_for_testing(
          autofill::AutofillProfile::Source::kAccount);
    }
    return std::make_unique<AutofillSaveUpdateAddressProfileDelegateIOS>(
        profile_, original_profile, email,
        /*locale=*/"en-US",
        AutofillClient::SaveAddressProfilePromptOptions{
            .is_migration_to_account = is_migration_to_account},
        callback_.Get());
  }

  AutofillProfile profile_;
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback>
      callback_;
};

// Tests that the callback is run with kAccepted on Accepted.
TEST_F(AutofillSaveUpdateAddressProfileDelegateIOSTest,
       HandleUserAction_Accepted) {
  std::unique_ptr<AutofillSaveUpdateAddressProfileDelegateIOS> delegate =
      CreateAutofillSaveUpdateAddressProfileDelegate();
  EXPECT_CALL(
      callback_,
      Run(AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted,
          Property(&profile_ref::has_value, false)));
  delegate->Accept();
}

// Tests that the callback is run with kDeclined on destruction.
TEST_F(AutofillSaveUpdateAddressProfileDelegateIOSTest,
       TestCallbackOnDestruction) {
  std::unique_ptr<AutofillSaveUpdateAddressProfileDelegateIOS> delegate =
      CreateAutofillSaveUpdateAddressProfileDelegate();

  delegate->Cancel();
  EXPECT_CALL(
      callback_,
      Run(AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined,
          Property(&profile_ref::has_value, false)));
  // The callback should run in the destructor.
  delegate.reset();
}

// Tests that the callback is run with kAccepted on Accept.
TEST_F(AutofillSaveUpdateAddressProfileDelegateIOSTest, TestCallbackOnSave) {
  std::unique_ptr<AutofillSaveUpdateAddressProfileDelegateIOS> delegate =
      CreateAutofillSaveUpdateAddressProfileDelegate();
  EXPECT_CALL(
      callback_,
      Run(AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted,
          Property(&profile_ref::has_value, false)));
  delegate->Accept();
}

// Tests that the callback is run with kEditAccepted on EditAccepted.
TEST_F(AutofillSaveUpdateAddressProfileDelegateIOSTest,
       TestCallbackOnEditAccepted) {
  std::unique_ptr<AutofillSaveUpdateAddressProfileDelegateIOS> delegate =
      CreateAutofillSaveUpdateAddressProfileDelegate();
  EXPECT_CALL(
      callback_,
      Run(AutofillClient::SaveAddressProfileOfferUserDecision::kEditAccepted,
          testing::_));
  delegate->EditAccepted();
}

// Tests that the callback is run with kNever on Never.
TEST_F(AutofillSaveUpdateAddressProfileDelegateIOSTest,
       HandleUserAction_NoThanks) {
  std::unique_ptr<AutofillSaveUpdateAddressProfileDelegateIOS> delegate =
      CreateAutofillSaveUpdateAddressProfileDelegate();
  EXPECT_CALL(callback_,
              Run(AutofillClient::SaveAddressProfileOfferUserDecision::kNever,
                  Property(&profile_ref::has_value, false)));
  delegate->Never();
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

  std::unique_ptr<AutofillSaveUpdateAddressProfileDelegateIOS> delegate =
      CreateAutofillSaveUpdateAddressProfileDelegate(
          is_update() ? &original_profile : nullptr, kTestEmail, is_migration(),
          is_account_profile());

  const DelegateStringsTestCase& test_case = GetParam();
  EXPECT_EQ(
      delegate->GetMessageActionText(),
      l10n_util::GetStringUTF16(test_case.expected_message_action_text_id));
  EXPECT_EQ(delegate->GetMessageText(),
            l10n_util::GetStringUTF16(test_case.expected_message_text_id));
  if (absl::holds_alternative<int>(test_case.expected_description_or_id)) {
    EXPECT_EQ(
        delegate->GetDescription(),
        l10n_util::GetStringFUTF16(
            absl::get<int>(test_case.expected_description_or_id), kTestEmail));
  } else {
    EXPECT_EQ(delegate->GetDescription(),
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

}  // namespace autofill
