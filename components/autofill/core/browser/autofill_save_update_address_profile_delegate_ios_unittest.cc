// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_save_update_address_profile_delegate_ios.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

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

  EXPECT_EQ(delegate->GetMessageActionText(), std::u16string(u"Save..."));
  EXPECT_EQ(delegate->GetMessageText(), std::u16string(u"Save Address?"));
  EXPECT_EQ(delegate->GetDescription(),
            std::u16string(u"John H. Doe, 666 Erebus St."));
}

// Tests that the delegate returns Update Address profile strings when the
// original_profile is supplied to the delegate.
TEST(AutofillSaveUpdateAddressProfileDelegateIOSTest,
     TestUpdateAddressStrings) {
  AutofillProfile profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  auto delegate = std::make_unique<AutofillSaveUpdateAddressProfileDelegateIOS>(
      profile, &profile, /*locale=*/"en-US", callback.Get());

  EXPECT_EQ(delegate->GetMessageActionText(), std::u16string(u"Update..."));
  EXPECT_EQ(delegate->GetMessageText(), std::u16string(u"Update Address?"));
  EXPECT_EQ(delegate->GetDescription(),
            std::u16string(u"John H. Doe \x2014 666 Erebus St."));
}

}  // namespace autofill
