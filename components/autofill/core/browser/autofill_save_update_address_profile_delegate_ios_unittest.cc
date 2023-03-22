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

class AutofillSaveUpdateAddressProfileDelegateIOSTest : public testing::Test {
 protected:
  AutofillSaveUpdateAddressProfileDelegateIOSTest() = default;
  ~AutofillSaveUpdateAddressProfileDelegateIOSTest() override {}

  void SetUp() override { profile_ = test::GetFullProfile(); }

  std::unique_ptr<AutofillSaveUpdateAddressProfileDelegateIOS>
  CreateAutofillSaveUpdateAddressProfileDelegate(
      AutofillProfile* original_profile = nullptr,
      absl::optional<std::u16string> email = absl::nullopt,
      bool is_migration_to_account = false) {
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
          profile_));
  delegate->Accept();
}

// Tests that the delegate returns Save Address profile strings when the
// original_profile is supplied as nullptr to the delegate.
TEST_F(AutofillSaveUpdateAddressProfileDelegateIOSTest,
       TestSaveAddressStrings) {
  std::unique_ptr<AutofillSaveUpdateAddressProfileDelegateIOS> delegate =
      CreateAutofillSaveUpdateAddressProfileDelegate();
  EXPECT_EQ(delegate->GetMessageActionText(),
            l10n_util::GetStringUTF16(
                IDS_IOS_AUTOFILL_SAVE_ADDRESS_MESSAGE_PRIMARY_ACTION));
  EXPECT_EQ(
      delegate->GetMessageText(),
      l10n_util::GetStringUTF16(IDS_IOS_AUTOFILL_SAVE_ADDRESS_MESSAGE_TITLE));
  EXPECT_EQ(delegate->GetDescription(),
            std::u16string(u"John H. Doe, 666 Erebus St."));
}

// Tests the message UI strings when the profile is saved in the Google Account.
TEST_F(AutofillSaveUpdateAddressProfileDelegateIOSTest,
       TestSaveAddressInAccountStrings) {
  std::unique_ptr<AutofillSaveUpdateAddressProfileDelegateIOS> delegate =
      CreateAutofillSaveUpdateAddressProfileDelegate(nullptr, u"test@gmail.com",
                                                     true);
  EXPECT_EQ(delegate->GetDescription(),
            l10n_util::GetStringFUTF16(
                IDS_IOS_AUTOFILL_SAVE_ADDRESS_IN_ACCOUNT_MESSAGE_SUBTITLE,
                u"test@gmail.com"));
  EXPECT_EQ(delegate->GetMessageText(),
            l10n_util::GetStringUTF16(
                IDS_IOS_AUTOFILL_SAVE_ADDRESS_IN_ACCOUNT_MESSAGE_TITLE));
}

// Tests that the delegate returns Update Address profile strings when the
// original_profile is supplied to the delegate.
TEST_F(AutofillSaveUpdateAddressProfileDelegateIOSTest,
       TestUpdateAddressStrings) {
  AutofillProfile original_profile = test::GetFullProfile();
  original_profile.SetInfo(NAME_FULL, u"John Doe", "en-US");
  std::unique_ptr<AutofillSaveUpdateAddressProfileDelegateIOS> delegate =
      CreateAutofillSaveUpdateAddressProfileDelegate(&original_profile);

  EXPECT_EQ(delegate->GetMessageActionText(),
            l10n_util::GetStringUTF16(
                IDS_IOS_AUTOFILL_UPDATE_ADDRESS_MESSAGE_PRIMARY_ACTION));
  EXPECT_EQ(
      delegate->GetMessageText(),
      l10n_util::GetStringUTF16(IDS_IOS_AUTOFILL_UPDATE_ADDRESS_MESSAGE_TITLE));
  EXPECT_EQ(delegate->GetDescription(),
            std::u16string(u"John Doe, 666 Erebus St."));
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
          testing::_));
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
          testing::_));
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

}  // namespace autofill
