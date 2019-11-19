// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_PERSONAL_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_PERSONAL_DATA_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "components/autofill/core/browser/personal_data_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

class MockPersonalDataManager : public autofill::PersonalDataManager {
 public:
  MockPersonalDataManager();
  ~MockPersonalDataManager() override;

  MOCK_METHOD1(SaveImportedProfile,
               std::string(const autofill::AutofillProfile&));
  MOCK_METHOD1(GetProfileByGUID,
               autofill::AutofillProfile*(const std::string&));
  MOCK_CONST_METHOD0(GetProfiles, std::vector<autofill::AutofillProfile*>());
  MOCK_CONST_METHOD0(GetCreditCards, std::vector<autofill::CreditCard*>());
  MOCK_CONST_METHOD0(IsAutofillProfileEnabled, bool());
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_PERSONAL_DATA_MANAGER_H_
