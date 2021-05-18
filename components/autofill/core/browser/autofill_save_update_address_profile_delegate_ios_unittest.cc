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
  AutofillProfile original_profile = test::GetFullProfile();
  original_profile.SetInfo(NAME_FULL, u"John Doe", "en-US");
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  auto delegate = std::make_unique<AutofillSaveUpdateAddressProfileDelegateIOS>(
      profile, &original_profile, /*locale=*/"en-US", callback.Get());

  EXPECT_EQ(delegate->GetMessageActionText(), std::u16string(u"Update..."));
  EXPECT_EQ(delegate->GetMessageText(), std::u16string(u"Update Address?"));
  EXPECT_EQ(delegate->GetDescription(),
            std::u16string(u"John Doe, 666 Erebus St."));
}

// Tests that delegate returns the correct profile difference.
TEST(AutofillSaveUpdateAddressProfileDelegateIOSTest, TestProfileDiff) {
  AutofillProfile profile = test::GetFullProfile();
  AutofillProfile original_profile = test::GetFullProfile2();
  original_profile.SetInfo(NAME_FULL, u"John Doe", "en-US");
  auto delegate = std::make_unique<AutofillSaveUpdateAddressProfileDelegateIOS>(
      profile, &original_profile, /*locale=*/"en-US", base::DoNothing());

  base::flat_map<ServerFieldType, std::pair<std::u16string, std::u16string>>
      expected_difference;
  expected_difference.insert({NAME_FULL, {u"John H. Doe", u"John Doe"}});
  expected_difference.insert(
      {EMAIL_ADDRESS, {u"johndoe@hades.com", u"jsmith@example.com"}});
  expected_difference.insert(
      {PHONE_HOME_WHOLE_NUMBER, {u"16502111111", u"13105557889"}});
  expected_difference.insert({ADDRESS_HOME_CITY, {u"Elysium", u"Greensdale"}});
  expected_difference.insert({ADDRESS_HOME_STATE, {u"CA", u"MI"}});
  expected_difference.insert({ADDRESS_HOME_ZIP, {u"91111", u"48838"}});
  expected_difference.insert({COMPANY_NAME, {u"Underworld", u"ACME"}});
  expected_difference.insert(
      {ADDRESS_HOME_STREET_ADDRESS,
       {u"666 Erebus St.\nApt 8", u"123 Main Street\nUnit 1"}});
  EXPECT_EQ(delegate->GetProfileDiff(), expected_difference);
}

}  // namespace autofill
