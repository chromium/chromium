// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_PAYMENTS_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_PAYMENTS_DATA_MANAGER_H_

#include "components/autofill/core/browser/payments_data_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace autofill {

// A simplistic PaymentsDataManager used for testing.
class TestPaymentsDataManager : public PaymentsDataManager {
 public:
  TestPaymentsDataManager(const std::string& app_locale,
                          PersonalDataManager* pdm);

  TestPaymentsDataManager(const TestPaymentsDataManager&) = delete;
  TestPaymentsDataManager& operator=(const TestPaymentsDataManager&) = delete;

  ~TestPaymentsDataManager() override;

  // PaymentsDataManager:
  void LoadCreditCards() override;
  void LoadCreditCardCloudTokenData() override;
  void LoadIbans() override;

  // Clears |local_credit_cards_| and |server_credit_cards_|.
  void ClearCreditCards();

  // Clears |autofill_offer_data_|.
  void ClearCreditCardOfferData();
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_PAYMENTS_DATA_MANAGER_H_
