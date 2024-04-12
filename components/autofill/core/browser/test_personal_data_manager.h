// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_PERSONAL_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_PERSONAL_DATA_MANAGER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_address_data_manager.h"
#include "components/autofill/core/browser/test_payments_data_manager.h"

namespace autofill {

// A simplistic PersonalDataManager used for testing. It doesn't load profiles
// from AutofillTable or update them there.
class TestPersonalDataManager : public PersonalDataManager {
 public:
  TestPersonalDataManager();

  TestPersonalDataManager(const TestPersonalDataManager&) = delete;
  TestPersonalDataManager& operator=(const TestPersonalDataManager&) = delete;

  ~TestPersonalDataManager() override;

  TestAddressDataManager& test_address_data_manager() {
    AddressDataManager& manager = address_data_manager();
    return static_cast<TestAddressDataManager&>(manager);
  }
  const TestAddressDataManager& test_address_data_manager() const {
    const AddressDataManager& manager = address_data_manager();
    return static_cast<const TestAddressDataManager&>(manager);
  }
  TestPaymentsDataManager& test_payments_data_manager() {
    PaymentsDataManager& manager = payments_data_manager();
    return static_cast<TestPaymentsDataManager&>(manager);
  }
  const TestPaymentsDataManager& test_payments_data_manager() const {
    const PaymentsDataManager& manager = payments_data_manager();
    return static_cast<const TestPaymentsDataManager&>(manager);
  }

  // Can be used to inject mock instances.
  void set_address_data_manager(
      std::unique_ptr<TestAddressDataManager> address_data_manager) {
    address_data_manager_ = std::move(address_data_manager);
  }
  void set_payments_data_manager(
      std::unique_ptr<TestPaymentsDataManager> payments_data_manager) {
    payments_data_manager_ = std::move(payments_data_manager);
  }

  // PersonalDataManager overrides.  These functions are overridden as needed
  // for various tests, whether to skip calls to uncreated databases/services,
  // or to make things easier in general to toggle.
  bool IsDataLoaded() const override;

  // Unique to TestPersonalDataManager:
  void SetPrefService(PrefService* pref_service);

  // Clears `web_profiles_` and `account_profiles_`.
  void ClearProfiles();

  // Clears all address and payments data.
  void ClearAllLocalData();

  // Adds a card to `server_credit_cards_`. This test class treats masked and
  // full server cards equally, relying on their preset RecordType to
  // differentiate them.
  void AddServerCreditCard(const CreditCard& credit_card);

  // Adds a cloud token data to |server_credit_card_cloud_token_data_|.
  void AddCloudTokenData(const CreditCardCloudTokenData& cloud_token_data);

  // Adds offer data to |autofill_offer_data_|.
  void AddAutofillOfferData(const AutofillOfferData& offer_data);

  // Adds an `iban` to `server_ibans_`.
  void AddServerIban(const Iban& iban);

  // Adds a `url` to `image` mapping to the local `credit_card_art_images_`
  // cache.
  void AddCardArtImage(const GURL& url, const gfx::Image& image);

  // Adds `usage_data` to `autofill_virtual_card_usage_data_`.
  void AddVirtualCardUsageData(const VirtualCardUsageData& usage_data);

  // Sets a local/server card's nickname based on the provided |guid|.
  void SetNicknameForCardWithGUID(std::string_view guid,
                                  std::string_view nickname);

  // TODO(b/322170538): Remove function from TestPDM.
  void SetAutofillPaymentMethodsEnabled(bool autofill_payment_methods_enabled) {
    test_payments_data_manager().SetAutofillPaymentMethodsEnabled(
        autofill_payment_methods_enabled);
  }

  // TODO(b/322170538): Remove function from TestPDM.
  void SetAutofillProfileEnabled(bool autofill_profile_enabled) {
    test_address_data_manager().SetAutofillProfileEnabled(
        autofill_profile_enabled);
  }

  // TODO(b/322170538): Remove function from TestPDM.
  void SetAutofillWalletImportEnabled(bool autofill_wallet_import_enabled) {
    test_payments_data_manager().SetAutofillWalletImportEnabled(
        autofill_wallet_import_enabled);
  }

  void SetPaymentsCustomerData(
      std::unique_ptr<PaymentsCustomerData> customer_data) {
    payments_data_manager_->payments_customer_data_ = std::move(customer_data);
  }

  void ClearCreditCardArtImages() {
    payments_data_manager_->credit_card_art_images_.clear();
  }
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_PERSONAL_DATA_MANAGER_H_
