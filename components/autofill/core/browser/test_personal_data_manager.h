// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_PERSONAL_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_PERSONAL_DATA_MANAGER_H_

#include <memory>
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
#include "components/autofill/core/browser/strike_databases/autofill_profile_migration_strike_database.h"
#include "components/autofill/core/browser/strike_databases/test_inmemory_strike_database.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill {

// A simplistic PersonalDataManager used for testing. It doesn't load profiles
// from AutofillTable or update them there.
class TestPersonalDataManager : public PersonalDataManager {
 public:
  TestPersonalDataManager();

  TestPersonalDataManager(const TestPersonalDataManager&) = delete;
  TestPersonalDataManager& operator=(const TestPersonalDataManager&) = delete;

  ~TestPersonalDataManager() override;

  using PersonalDataManager::GetProfileSaveStrikeDatabase;
  using PersonalDataManager::GetProfileUpdateStrikeDatabase;
  using PersonalDataManager::SetPrefService;

  // PersonalDataManager overrides.  These functions are overridden as needed
  // for various tests, whether to skip calls to uncreated databases/services,
  // or to make things easier in general to toggle.
  bool IsPaymentsWalletSyncTransportEnabled() const override;
  void RecordUseOf(absl::variant<const AutofillProfile*, const CreditCard*>
                       profile_or_credit_card) override;
  std::string SaveImportedCreditCard(
      const CreditCard& imported_credit_card) override;
  void AddProfile(const AutofillProfile& profile) override;
  void UpdateProfile(const AutofillProfile& profile) override;
  void RemoveByGUID(const std::string& guid) override;
  bool IsEligibleForAddressAccountStorage() const override;
  void AddCreditCard(const CreditCard& credit_card) override;
  std::string AddIban(const Iban iban) override;
  std::string UpdateIban(const Iban& iban) override;
  void DeleteLocalCreditCards(const std::vector<CreditCard>& cards) override;
  void UpdateCreditCard(const CreditCard& credit_card) override;
  void AddFullServerCreditCard(const CreditCard& credit_card) override;
  const std::string& GetDefaultCountryCodeForNewAddress() const override;
  void LoadProfiles() override;
  void LoadCreditCards() override;
  void LoadCreditCardCloudTokenData() override;
  void LoadIbans() override;
  bool IsAutofillProfileEnabled() const override;
  bool IsAutofillPaymentMethodsEnabled() const override;
  bool IsAutofillWalletImportEnabled() const override;
  bool ShouldSuggestServerCards() const override;
  std::string CountryCodeForCurrentTimezone() const override;
  void ClearAllLocalData() override;
  bool IsDataLoaded() const override;
  bool IsSyncFeatureEnabledForPaymentsServerMetrics() const override;
  CoreAccountInfo GetAccountInfoForPaymentsServer() const override;
  const AutofillProfileMigrationStrikeDatabase*
  GetProfileMigrationStrikeDatabase() const override;
  const AutofillProfileSaveStrikeDatabase* GetProfileSaveStrikeDatabase()
      const override;
  const AutofillProfileUpdateStrikeDatabase* GetProfileUpdateStrikeDatabase()
      const override;
  bool IsPaymentMethodsMandatoryReauthEnabled() override;
  void SetPaymentMethodsMandatoryReauthEnabled(bool enabled) override;

  // Unique to TestPersonalDataManager:

  // Clears `web_profiles_` and `account_profiles_`.
  void ClearProfiles();

  // Clears |local_credit_cards_| and |server_credit_cards_|.
  void ClearCreditCards();

  // Clears |server_credit_card_cloud_token_data_|.
  void ClearCloudTokenData();

  // Clears |autofill_offer_data_|.
  void ClearCreditCardOfferData();

  // Adds a card to |server_credit_cards_|.  Functionally identical to
  // AddFullServerCreditCard().
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

  void set_timezone_country_code(const std::string& timezone_country_code) {
    timezone_country_code_ = timezone_country_code;
  }
  void set_default_country_code(const std::string& default_country_code) {
    default_country_code_ = default_country_code;
  }

  int num_times_save_imported_credit_card_called() const {
    return num_times_save_imported_credit_card_called_;
  }

  void SetAutofillPaymentMethodsEnabled(bool autofill_payment_methods_enabled) {
    autofill_payment_methods_enabled_ = autofill_payment_methods_enabled;
  }

  void SetAutofillProfileEnabled(bool autofill_profile_enabled) {
    autofill_profile_enabled_ = autofill_profile_enabled;
  }

  void SetAutofillWalletImportEnabled(bool autofill_wallet_import_enabled) {
    autofill_wallet_import_enabled_ = autofill_wallet_import_enabled;
  }

  void SetIsEligibleForAddressAccountStorage(bool eligible) {
    eligible_for_account_storage_ = eligible;
  }

  void SetPaymentsCustomerData(
      std::unique_ptr<PaymentsCustomerData> customer_data) {
    payments_customer_data_ = std::move(customer_data);
  }

  void SetIsPaymentsWalletSyncTransportEnabled(bool enabled) {
    payments_wallet_sync_transport_enabled_ = enabled;
  }

  void SetAccountInfoForPayments(const CoreAccountInfo& account_info) {
    account_info_ = account_info;
  }

  void ClearCreditCardArtImages() { credit_card_art_images_.clear(); }

 private:
  // This should be called when you just want to delete the element via `guid`
  // and not trigger `NotifyPersonalDataObserver()`.
  void RemoveByGuidWithoutNotifications(const std::string& guid);

  std::string timezone_country_code_;
  std::string default_country_code_;
  int num_times_save_imported_credit_card_called_ = 0;
  absl::optional<bool> autofill_profile_enabled_;
  absl::optional<bool> autofill_payment_methods_enabled_;
  absl::optional<bool> autofill_wallet_import_enabled_;
  absl::optional<bool> eligible_for_account_storage_;
  absl::optional<bool> payment_methods_mandatory_reauth_enabled_;
  absl::optional<bool> payments_wallet_sync_transport_enabled_;
  CoreAccountInfo account_info_;

  TestInMemoryStrikeDatabase inmemory_strike_database_;
  AutofillProfileMigrationStrikeDatabase
      inmemory_profile_migration_strike_database_{&inmemory_strike_database_};
  AutofillProfileSaveStrikeDatabase inmemory_profile_save_strike_database_{
      &inmemory_strike_database_};
  AutofillProfileUpdateStrikeDatabase inmemory_profile_update_strike_database_{
      &inmemory_strike_database_};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_PERSONAL_DATA_MANAGER_H_
