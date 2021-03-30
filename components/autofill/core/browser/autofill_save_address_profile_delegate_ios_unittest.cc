// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_save_address_profile_delegate_ios.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TEST(AutofillSaveAddressProfileDelegateIOSTest, HandleUserAction_Accepted) {
  AutofillProfile profile = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback> callback;
  auto delegate = std::make_unique<AutofillSaveAddressProfileDelegateIOS>(
      profile, callback.Get());

  EXPECT_CALL(
      callback,
      Run(AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted,
          profile));
  delegate->Accept();
}

}  // namespace autofill
