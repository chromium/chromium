// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_TEST_API_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/personal_data_manager.h"

namespace autofill {

// Provides testing functions for `PersonalDataManager`.
class PersonalDataManagerTestApi {
 public:
  explicit PersonalDataManagerTestApi(
      PersonalDataManager& personal_data_manager)
      : personal_data_manager_(personal_data_manager) {}

  // Returns the number of credit card benefits.
  size_t GetCreditCardBenefitsCount() {
    return personal_data_manager_->payments_data_manager_->credit_card_benefits_
        .size();
  }

 private:
  const raw_ref<PersonalDataManager> personal_data_manager_;
};

inline PersonalDataManagerTestApi test_api(
    PersonalDataManager& personal_data_manager) {
  return PersonalDataManagerTestApi(personal_data_manager);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_TEST_API_H_
