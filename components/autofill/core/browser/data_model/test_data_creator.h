// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_TEST_DATA_CREATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_TEST_DATA_CREATOR_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"

namespace autofill {

class TestDataCreator {
 public:
  TestDataCreator(base::TimeDelta cc_deletion_delta, std::string app_locale);

  void MaybeAddTestProfiles(
      const base::RepeatingCallback<void(const AutofillProfile&)>&
          add_profile_callback);
  void MaybeAddTestCreditCards(
      const base::RepeatingCallback<void(const CreditCard&)>& add_cc_callback);

 private:
  std::vector<AutofillProfile> GetTestProfiles();
  std::vector<CreditCard> GetTestCreditCards();

  AutofillProfile CreateBasicTestAddress();
  AutofillProfile CreateDisusedTestAddress();
  AutofillProfile CreateDisusedDeletableTestAddress();

  CreditCard CreateBasicTestCreditCard();
  CreditCard CreateDisusedTestCreditCard();
  CreditCard CreateDisusedDeletableTestCreditCard();

  // Minimum amount of time since last use for a credit card to be considered
  // "disused".
  base::TimeDelta cc_deletion_delta_;

  std::string app_locale_;

  // True if test data has been created this session.
  bool has_created_test_addresses_ = false;
  bool has_created_test_credit_cards_ = false;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_TEST_DATA_CREATOR_H_