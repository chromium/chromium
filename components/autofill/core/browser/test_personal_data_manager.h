// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_PERSONAL_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_PERSONAL_DATA_MANAGER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_inmemory_strike_database.h"
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
  void OnSyncServiceInitialized(syncer::SyncService* sync_service) override;
  AutofillSyncSigninState GetSyncSigninState() const override;
  void RecordUseOf(absl::variant<const AutofillProfile*, const CreditCard*>
                       profile_or_credit_card) override;
  std::string SaveImportedProfile(
      const AutofillProfile& imported_profile) override;
  std::string SaveImportedCreditCard(
      const CreditCard& imported_credit_card) override;
  void AddUpiId(const std::string& upi_id) override;
  void AddProfile(const AutofillProfile& profile) override;
  void UpdateProfile(const AutofillProfile& profile) override;
  void RemoveByGUID(const std::string& guid) override;
  void AddCreditCard(const CreditCard& credit_card) override;
  std::string AddIBAN(const IBAN& iban) override;
  void DeleteLocalCreditCards(const std::vector<CreditCard>& cards) override;
  void UpdateCreditCard(const CreditCard& credit_card) override;
  void AddFullServerCreditCard(const CreditCard& credit_card) override;
  const std::string& GetDefaultCountryCodeForNewAddress() const override;
  void SetProfilesForAllSources(
      std::vector<AutofillProfile>* profiles) override;
  bool SetProfilesForSource(base::span<const AutofillProfile> new_profiles,
                            AutofillProfile::Source source) override;
  void LoadProfiles() override;
  void LoadCreditCards() override;
  void LoadCreditCardCloudTokenData() override;
  void LoadIBANs() override;
  void LoadUpiIds() override;
  bool IsAutofillProfileEnabled() const override;
  bool IsAutofillCreditCardEnabled() const override;
  bool IsAutofillWalletImportEnabled() const override;
  bool ShouldSuggestServerCards() const override;
  std::string CountryCodeForCurrentTimezone() const override;
  void ClearAllLocalData() override;
  CreditCard* GetCreditCardByNumber(const std::string& number) override;
  bool IsDataLoaded() const override;
  bool IsSyncFeatureEnabled() const override;
  CoreAccountInfo GetAccountInfoForPaymentsServer() const override;
  const AutofillProfileSaveStrikeDatabase* GetProfileSaveStrikeDatabase()
      const override;
  const AutofillProfileUpdateStrikeDatabase* GetProfileUpdateStrikeDatabase()
      const override;

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

  // Adds a `url` to `image` mapping to the local `credit_card_art_images_`
  // cache.
  void AddCardArtImage(const GURL& url, const gfx::Image& image);

  // Sets a local/server card's nickname based on the provided |guid|.
  void SetNicknameForCardWithGUID(const char* guid,
                                  const std::string& nickname);

  void set_timezone_country_code(const std::string& timezone_country_code) {
    timezone_country_code_ = timezone_country_code;
  }
  void set_default_country_code(const std::string& default_country_code) {
    default_country_code_ = default_country_code;
  }

  int num_times_save_imported_profile_called() const {
    return num_times_save_imported_profile_called_;
  }

  int num_times_save_imported_credit_card_called() const {
    return num_times_save_imported_credit_card_called_;
  }

  int num_times_save_upi_id_called() const {
    return num_times_save_upi_id_called_;
  }

  bool sync_service_initialized() const { return sync_service_initialized_; }

  void SetAutofillCreditCardEnabled(bool autofill_credit_card_enabled) {
    autofill_credit_card_enabled_ = autofill_credit_card_enabled;
  }

  void SetAutofillProfileEnabled(bool autofill_profile_enabled) {
    autofill_profile_enabled_ = autofill_profile_enabled;
  }

  void SetAutofillWalletImportEnabled(bool autofill_wallet_import_enabled) {
    autofill_wallet_import_enabled_ = autofill_wallet_import_enabled;
  }

  void SetPaymentsCustomerData(
      std::unique_ptr<PaymentsCustomerData> customer_data) {
    payments_customer_data_ = std::move(customer_data);
  }

  void SetSyncFeatureEnabled(bool enabled) { sync_feature_enabled_ = enabled; }

  void SetSyncAndSignInState(AutofillSyncSigninState sync_and_signin_state) {
    sync_and_signin_state_ = sync_and_signin_state;
  }

  void SetAccountInfoForPayments(const CoreAccountInfo& account_info) {
    account_info_ = account_info;
  }

  void ClearCreditCardArtImages() { credit_card_art_images_.clear(); }

 private:
  std::string timezone_country_code_;
  std::string default_country_code_;
  int num_times_save_imported_profile_called_ = 0;
  int num_times_save_imported_credit_card_called_ = 0;
  int num_times_save_upi_id_called_ = 0;
  absl::optional<bool> autofill_profile_enabled_;
  absl::optional<bool> autofill_credit_card_enabled_;
  absl::optional<bool> autofill_wallet_import_enabled_;
  bool sync_feature_enabled_ = false;
  AutofillSyncSigninState sync_and_signin_state_ =
      AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled;
  bool sync_service_initialized_ = false;
  CoreAccountInfo account_info_;

  TestInMemoryStrikeDatabase inmemory_strike_database_;
  AutofillProfileSaveStrikeDatabase inmemory_profile_save_strike_database_{
      &inmemory_strike_database_};
  AutofillProfileUpdateStrikeDatabase inmemory_profile_update_strike_database_{
      &inmemory_strike_database_};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_PERSONAL_DATA_MANAGER_H_
