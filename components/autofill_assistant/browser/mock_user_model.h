// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_USER_MODEL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_USER_MODEL_H_

#include "components/autofill_assistant/browser/user_model.h"

#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

class MockUserModel : public UserModel {
 public:
  MockUserModel();
  ~MockUserModel() override;

  MOCK_METHOD(void,
              SetSelectedCreditCard,
              (std::unique_ptr<autofill::CreditCard> card, UserData* user_data),
              (override));

  MOCK_METHOD(void,
              SetSelectedAutofillProfile,
              (const std::string& profile_name,
               std::unique_ptr<autofill::AutofillProfile> profile,
               UserData* user_data),
              (override));
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_USER_MODEL_H_
