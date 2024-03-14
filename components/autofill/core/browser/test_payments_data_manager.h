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

  using PaymentsDataManager::SetPrefService;

  // PaymentsDataManager:
  void LoadCreditCards() override;
  void LoadCreditCardCloudTokenData() override;
  void LoadIbans() override;
  bool RemoveByGUID(const std::string& guid) override;
  void RecordUseOfCard(const CreditCard* card) override;
  void RecordUseOfIban(Iban& iban) override;
  void AddCreditCard(const CreditCard& credit_card) override;
  std::string AddAsLocalIban(const Iban iban) override;
  std::string UpdateIban(const Iban& iban) override;
  void DeleteLocalCreditCards(const std::vector<CreditCard>& cards) override;
  void UpdateCreditCard(const CreditCard& credit_card) override;
  void AddServerCvc(int64_t instrument_id, const std::u16string& cvc) override;
  void ClearServerCvcs() override;
  void ClearLocalCvcs() override;
  bool IsAutofillPaymentMethodsEnabled() const override;

  // Clears |local_credit_cards_| and |server_credit_cards_|.
  void ClearCreditCards();

  // Clears |autofill_offer_data_|.
  void ClearCreditCardOfferData();

  void SetAutofillPaymentMethodsEnabled(bool autofill_payment_methods_enabled) {
    autofill_payment_methods_enabled_ = autofill_payment_methods_enabled;
  }

 private:
  void RemoveCardWithoutNotification(const CreditCard& card);

  std::optional<bool> autofill_payment_methods_enabled_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_PAYMENTS_DATA_MANAGER_H_
