// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_PAYMENTS_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_PAYMENTS_DATA_MANAGER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "components/autofill/core/browser/data_model/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/payments_data_manager.h"

namespace autofill {

// A simplistic PaymentsDataManager used for testing.
// See TestPersonalDataManager.
class TestPaymentsDataManager : public PaymentsDataManager {
 public:
  explicit TestPaymentsDataManager(const std::string& app_locale = "en-US");

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
  bool IsAutofillWalletImportEnabled() const override;
  bool IsPaymentsWalletSyncTransportEnabled() const override;
  bool ShouldSuggestServerPaymentMethods() const override;
  bool IsPaymentMethodsMandatoryReauthEnabled() override;
  void SetPaymentMethodsMandatoryReauthEnabled(bool enabled) override;
  std::string SaveImportedCreditCard(
      const CreditCard& imported_credit_card) override;
  bool IsPaymentCvcStorageEnabled() override;
  bool IsSyncFeatureEnabledForPaymentsServerMetrics() const override;
  CoreAccountInfo GetAccountInfoForPaymentsServer() const override;

  // Clears |local_credit_cards_| and |server_credit_cards_|.
  void ClearCreditCards();

  // Clears |autofill_offer_data_|.
  void ClearCreditCardOfferData();

  // Clears all local payments data.
  void ClearAllLocalData();

  void SetAutofillPaymentMethodsEnabled(bool autofill_payment_methods_enabled) {
    autofill_payment_methods_enabled_ = autofill_payment_methods_enabled;
  }

  void SetAutofillWalletImportEnabled(bool autofill_wallet_import_enabled) {
    autofill_wallet_import_enabled_ = autofill_wallet_import_enabled;
  }

  void SetIsPaymentsWalletSyncTransportEnabled(bool enabled) {
    payments_wallet_sync_transport_enabled_ = enabled;
  }

  void SetIsPaymentCvcStorageEnabled(bool enabled) {
    payments_cvc_storage_enabled_ = enabled;
  }

  // Adds a card to `server_credit_cards_`. This test class treats masked and
  // full server cards equally, relying on their preset RecordType to
  // differentiate them.
  void AddServerCreditCard(const CreditCard& credit_card);

  // Adds offer data to `autofill_offer_data_`.
  void AddAutofillOfferData(const AutofillOfferData& offer_data);

  // Adds an `iban` to `server_ibans_`.
  void AddServerIban(const Iban& iban);

  void AddIbanForTest(std::unique_ptr<Iban> iban) {
    local_ibans_.push_back(std::move(iban));
  }

  // Adds a `url` to `image` mapping to the local `credit_card_art_images_`
  // cache.
  void AddCardArtImage(const GURL& url, const gfx::Image& image);

  void ClearCreditCardArtImages() { credit_card_art_images_.clear(); }

  // Adds `usage_data` to `autofill_virtual_card_usage_data_`.
  void AddVirtualCardUsageData(const VirtualCardUsageData& usage_data);

  // Sets a local/server card's nickname based on the provided `guid`.
  void SetNicknameForCardWithGUID(std::string_view guid,
                                  std::string_view nickname);

  void SetPaymentsCustomerData(
      std::unique_ptr<PaymentsCustomerData> customer_data) {
    payments_customer_data_ = std::move(customer_data);
  }

  void SetAccountInfoForPayments(const CoreAccountInfo& account_info) {
    account_info_ = account_info;
  }

 private:
  void RemoveCardWithoutNotification(const CreditCard& card);

  std::optional<bool> autofill_payment_methods_enabled_;
  std::optional<bool> autofill_wallet_import_enabled_;
  std::optional<bool> payments_wallet_sync_transport_enabled_;
  std::optional<bool> payment_methods_mandatory_reauth_enabled_;
  std::optional<bool> payments_cvc_storage_enabled_;
  CoreAccountInfo account_info_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_PAYMENTS_DATA_MANAGER_H_
