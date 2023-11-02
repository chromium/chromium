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

TEST(AutofillSaveUpdateAddressProfileDelegateIOSTest,
     HandleUserAction_Accepted) {
  AutofillProfile profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  auto delegate = std::make_unique<AutofillSaveUpdateAddressProfileDelegateIOS>(
      profile, /*original_profile=*/nullptr, /*locale=*/"en-US",
      callback.Get());

  EXPECT_CALL(
      callback,
      Run(AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted,
          profile));
  delegate->Accept();
}

// Tests that the delegate returns Save Address profile strings when the
// original_profile is supplied as nullptr to the delegate.
TEST(AutofillSaveUpdateAddressProfileDelegateIOSTest, TestSaveAddressStrings) {
  AutofillProfile profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  auto delegate = std::make_unique<AutofillSaveUpdateAddressProfileDelegateIOS>(
      profile, /*original_profile=*/nullptr, /*locale=*/"en-US",
      callback.Get());

  EXPECT_EQ(delegate->GetMessageActionText(),
            l10n_util::GetStringUTF16(
                IDS_IOS_AUTOFILL_SAVE_ADDRESS_MESSAGE_PRIMARY_ACTION));
  EXPECT_EQ(
      delegate->GetMessageText(),
      l10n_util::GetStringUTF16(IDS_IOS_AUTOFILL_SAVE_ADDRESS_MESSAGE_TITLE));
  EXPECT_EQ(delegate->GetDescription(),
            std::u16string(u"John H. Doe, 666 Erebus St."));
}

// Tests that the delegate returns Update Address profile strings when the
// original_profile is supplied to the delegate.
TEST(AutofillSaveUpdateAddressProfileDelegateIOSTest,
     TestUpdateAddressStrings) {
  AutofillProfile profile = test::GetFullProfile();
  AutofillProfile original_profile = test::GetFullProfile();
  original_profile.SetInfo(NAME_FULL, u"John Doe", "en-US");
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  auto delegate = std::make_unique<AutofillSaveUpdateAddressProfileDelegateIOS>(
      profile, &original_profile, /*locale=*/"en-US", callback.Get());

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
TEST(AutofillSaveUpdateAddressProfileDelegateIOSTest,
     TestCallbackOnDestruction) {
  AutofillProfile profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  auto delegate = std::make_unique<AutofillSaveUpdateAddressProfileDelegateIOS>(
      profile, /*original_profile=*/nullptr, /*locale=*/"en-US",
      callback.Get());

  delegate->Cancel();
  EXPECT_CALL(
      callback,
      Run(AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined,
          testing::_));
  // The callback should run in the destructor.
  delegate.reset();
}

// Tests that the callback is run with kAccepted on Accept.
TEST(AutofillSaveUpdateAddressProfileDelegateIOSTest, TestCallbackOnSave) {
  AutofillProfile profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  EXPECT_CALL(
      callback,
      Run(AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted,
          testing::_));
  AutofillSaveUpdateAddressProfileDelegateIOS(
      profile, /*original_profile=*/nullptr, /*locale=*/"en-US", callback.Get())
      .Accept();
}

// Tests that the callback is run with kEditAccepted on EditAccepted.
TEST(AutofillSaveUpdateAddressProfileDelegateIOSTest,
     TestCallbackOnEditAccepted) {
  AutofillProfile profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  EXPECT_CALL(
      callback,
      Run(AutofillClient::SaveAddressProfileOfferUserDecision::kEditAccepted,
          testing::_));
  AutofillSaveUpdateAddressProfileDelegateIOS(
      profile, /*original_profile=*/nullptr, /*locale=*/"en-US", callback.Get())
      .EditAccepted();
}

}  // namespace autofill
