// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_personal_data_manager.h"

#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

TestPersonalDataManager::TestPersonalDataManager()
    : PersonalDataManager("en-US", "US") {}

TestPersonalDataManager::~TestPersonalDataManager() = default;

void TestPersonalDataManager::OnSyncServiceInitialized(
    syncer::SyncService* sync_service) {
  sync_service_initialized_ = true;
}

AutofillSyncSigninState TestPersonalDataManager::GetSyncSigninState() const {
  return sync_and_signin_state_;
}

void TestPersonalDataManager::RecordUseOf(
    absl::variant<const AutofillProfile*, const CreditCard*>
        profile_or_credit_card) {
  if (absl::holds_alternative<const CreditCard*>(profile_or_credit_card)) {
    CreditCard* credit_card = GetCreditCardByGUID(
        absl::get<const CreditCard*>(profile_or_credit_card)->guid());

    if (credit_card)
      credit_card->RecordAndLogUse();
  }

  if (absl::holds_alternative<const AutofillProfile*>(profile_or_credit_card)) {
    AutofillProfile* profile = GetProfileByGUID(
        absl::get<const AutofillProfile*>(profile_or_credit_card)->guid());

    if (profile)
      profile->RecordAndLogUse();
  }
}

std::string TestPersonalDataManager::SaveImportedProfile(
    const AutofillProfile& imported_profile) {
  num_times_save_imported_profile_called_++;
  return PersonalDataManager::SaveImportedProfile(imported_profile);
}

std::string TestPersonalDataManager::SaveImportedCreditCard(
    const CreditCard& imported_credit_card) {
  num_times_save_imported_credit_card_called_++;
  AddCreditCard(imported_credit_card);
  return imported_credit_card.guid();
}

void TestPersonalDataManager::AddUpiId(const std::string& profile) {
  num_times_save_upi_id_called_++;
}

void TestPersonalDataManager::AddProfile(const AutofillProfile& profile) {
  std::unique_ptr<AutofillProfile> profile_ptr =
      std::make_unique<AutofillProfile>(profile);
  profile_ptr->FinalizeAfterImport();
  GetProfileStorage(profile.source()).push_back(std::move(profile_ptr));
  NotifyPersonalDataObserver();
}

void TestPersonalDataManager::UpdateProfile(const AutofillProfile& profile) {
  AutofillProfile* existing_profile = GetProfileByGUID(profile.guid());
  if (existing_profile) {
    RemoveByGUID(existing_profile->guid());
    AddProfile(profile);
  }
}

void TestPersonalDataManager::RemoveByGUID(const std::string& guid) {
  CreditCard* credit_card = GetCreditCardByGUID(guid);
  if (credit_card) {
    local_credit_cards_.erase(base::ranges::find(
        local_credit_cards_, credit_card, &std::unique_ptr<CreditCard>::get));
  }

  AutofillProfile* profile = GetProfileByGUID(guid);
  if (profile) {
    std::vector<std::unique_ptr<AutofillProfile>>& profiles =
        GetProfileStorage(profile->source());
    profiles.erase(base::ranges::find(profiles, profile,
                                      &std::unique_ptr<AutofillProfile>::get));
  }
}

void TestPersonalDataManager::AddCreditCard(const CreditCard& credit_card) {
  std::unique_ptr<CreditCard> local_credit_card =
      std::make_unique<CreditCard>(credit_card);
  local_credit_cards_.push_back(std::move(local_credit_card));
  NotifyPersonalDataObserver();
}

std::string TestPersonalDataManager::AddIBAN(const IBAN& iban) {
  std::unique_ptr<IBAN> local_iban = std::make_unique<IBAN>(iban);
  local_ibans_.push_back(std::move(local_iban));
  NotifyPersonalDataObserver();
  return iban.guid();
}

void TestPersonalDataManager::DeleteLocalCreditCards(
    const std::vector<CreditCard>& cards) {
  for (const auto& card : cards)
    RemoveByGUID(card.guid());

  NotifyPersonalDataObserver();
}

void TestPersonalDataManager::UpdateCreditCard(const CreditCard& credit_card) {
  CreditCard* existing_credit_card = GetCreditCardByGUID(credit_card.guid());
  if (existing_credit_card) {
    RemoveByGUID(existing_credit_card->guid());
    AddCreditCard(credit_card);
  }
}

void TestPersonalDataManager::AddFullServerCreditCard(
    const CreditCard& credit_card) {
  // Though the name is AddFullServerCreditCard, this test class treats masked
  // and full server cards equally, relying on their preset RecordType to
  // differentiate them.
  AddServerCreditCard(credit_card);
}

const std::string& TestPersonalDataManager::GetDefaultCountryCodeForNewAddress()
    const {
  if (default_country_code_.empty())
    return PersonalDataManager::GetDefaultCountryCodeForNewAddress();

  return default_country_code_;
}

void TestPersonalDataManager::SetProfilesForAllSources(
    std::vector<AutofillProfile>* profiles) {
  // Copy all the profiles. Called by functions like
  // PersonalDataManager::SaveImportedProfile, which impact metrics.
  ClearProfiles();
  for (const auto& profile : *profiles)
    AddProfile(profile);
}

bool TestPersonalDataManager::SetProfilesForSource(
    base::span<const AutofillProfile> new_profiles,
    AutofillProfile::Source source) {
  GetProfileStorage(source).clear();
  for (const AutofillProfile& profile : new_profiles)
    AddProfile(profile);
  return true;
}

void TestPersonalDataManager::LoadProfiles() {
  // Overridden to avoid a trip to the database. This should be a no-op except
  // for the side-effect of logging the profile count.
  pending_synced_local_profiles_query_ = 123;
  pending_account_profiles_query_ = 124;
  pending_creditcard_billing_addresses_query_ = 125;
  {
    std::vector<std::unique_ptr<AutofillProfile>> profiles;
    synced_local_profiles_.swap(profiles);
    auto result = std::make_unique<
        WDResult<std::vector<std::unique_ptr<AutofillProfile>>>>(
        AUTOFILL_PROFILES_RESULT, std::move(profiles));
    OnWebDataServiceRequestDone(pending_synced_local_profiles_query_,
                                std::move(result));
  }
  if (base::FeatureList::IsEnabled(
          features::kAutofillAccountProfilesUnionView)) {
    std::vector<std::unique_ptr<AutofillProfile>> profiles;
    account_profiles_.swap(profiles);
    auto result = std::make_unique<
        WDResult<std::vector<std::unique_ptr<AutofillProfile>>>>(
        AUTOFILL_PROFILES_RESULT, std::move(profiles));
    OnWebDataServiceRequestDone(pending_account_profiles_query_,
                                std::move(result));
  }
  {
    std::vector<std::unique_ptr<AutofillProfile>> profiles;
    credit_card_billing_addresses_.swap(profiles);
    auto result = std::make_unique<
        WDResult<std::vector<std::unique_ptr<AutofillProfile>>>>(
        AUTOFILL_PROFILES_RESULT, std::move(profiles));
    OnWebDataServiceRequestDone(pending_creditcard_billing_addresses_query_,
                                std::move(result));
  }
}

void TestPersonalDataManager::LoadCreditCards() {
  // Overridden to avoid a trip to the database.
  pending_creditcards_query_ = 125;
  pending_server_creditcards_query_ = 126;
  {
    std::vector<std::unique_ptr<CreditCard>> credit_cards;
    local_credit_cards_.swap(credit_cards);
    std::unique_ptr<WDTypedResult> result =
        std::make_unique<WDResult<std::vector<std::unique_ptr<CreditCard>>>>(
            AUTOFILL_CREDITCARDS_RESULT, std::move(credit_cards));
    OnWebDataServiceRequestDone(pending_creditcards_query_, std::move(result));
  }
  {
    std::vector<std::unique_ptr<CreditCard>> credit_cards;
    server_credit_cards_.swap(credit_cards);
    std::unique_ptr<WDTypedResult> result =
        std::make_unique<WDResult<std::vector<std::unique_ptr<CreditCard>>>>(
            AUTOFILL_CREDITCARDS_RESULT, std::move(credit_cards));
    OnWebDataServiceRequestDone(pending_server_creditcards_query_,
                                std::move(result));
  }
}

void TestPersonalDataManager::LoadCreditCardCloudTokenData() {
  pending_server_creditcard_cloud_token_data_query_ = 127;
  {
    std::vector<std::unique_ptr<CreditCardCloudTokenData>> cloud_token_data;
    server_credit_card_cloud_token_data_.swap(cloud_token_data);
    std::unique_ptr<WDTypedResult> result = std::make_unique<
        WDResult<std::vector<std::unique_ptr<CreditCardCloudTokenData>>>>(
        AUTOFILL_CLOUDTOKEN_RESULT, std::move(cloud_token_data));
    OnWebDataServiceRequestDone(
        pending_server_creditcard_cloud_token_data_query_, std::move(result));
  }
}

void TestPersonalDataManager::LoadIBANs() {
  pending_ibans_query_ = 128;
  {
    std::vector<std::unique_ptr<IBAN>> ibans;
    local_ibans_.swap(ibans);
    std::unique_ptr<WDTypedResult> result =
        std::make_unique<WDResult<std::vector<std::unique_ptr<IBAN>>>>(
            AUTOFILL_IBANS_RESULT, std::move(ibans));
    OnWebDataServiceRequestDone(pending_ibans_query_, std::move(result));
  }
}

void TestPersonalDataManager::LoadUpiIds() {
  pending_upi_ids_query_ = 129;
  {
    std::vector<std::string> upi_ids = {"vpa@indianbank"};
    std::unique_ptr<WDTypedResult> result =
        std::make_unique<WDResult<std::vector<std::string>>>(
            AUTOFILL_UPI_RESULT, std::move(upi_ids));
    OnWebDataServiceRequestDone(pending_upi_ids_query_, std::move(result));
  }
}

bool TestPersonalDataManager::IsAutofillProfileEnabled() const {
  // Return the value of autofill_profile_enabled_ if it has been set,
  // otherwise fall back to the normal behavior of checking the pref_service.
  if (autofill_profile_enabled_.has_value())
    return autofill_profile_enabled_.value();
  return PersonalDataManager::IsAutofillProfileEnabled();
}

bool TestPersonalDataManager::IsAutofillCreditCardEnabled() const {
  // Return the value of autofill_credit_card_enabled_ if it has been set,
  // otherwise fall back to the normal behavior of checking the pref_service.
  if (autofill_credit_card_enabled_.has_value())
    return autofill_credit_card_enabled_.value();
  return PersonalDataManager::IsAutofillCreditCardEnabled();
}

bool TestPersonalDataManager::IsAutofillWalletImportEnabled() const {
  // Return the value of autofill_wallet_import_enabled_ if it has been set,
  // otherwise fall back to the normal behavior of checking the pref_service.
  if (autofill_wallet_import_enabled_.has_value())
    return autofill_wallet_import_enabled_.value();
  return PersonalDataManager::IsAutofillWalletImportEnabled();
}

bool TestPersonalDataManager::ShouldSuggestServerCards() const {
  return IsAutofillCreditCardEnabled() && IsAutofillWalletImportEnabled();
}

std::string TestPersonalDataManager::CountryCodeForCurrentTimezone() const {
  return timezone_country_code_;
}

void TestPersonalDataManager::ClearAllLocalData() {
  ClearProfiles();
  local_credit_cards_.clear();
}

CreditCard* TestPersonalDataManager::GetCreditCardByNumber(
    const std::string& number) {
  CreditCard numbered_card;
  numbered_card.SetNumber(base::UTF8ToUTF16(number));
  for (CreditCard* credit_card : GetCreditCards()) {
    DCHECK(credit_card);
    if (credit_card->HasSameNumberAs(numbered_card))
      return credit_card;
  }
  return nullptr;
}

bool TestPersonalDataManager::IsDataLoaded() const {
  return true;
}

bool TestPersonalDataManager::IsSyncFeatureEnabled() const {
  return sync_feature_enabled_;
}

CoreAccountInfo TestPersonalDataManager::GetAccountInfoForPaymentsServer()
    const {
  return account_info_;
}

const AutofillProfileSaveStrikeDatabase*
TestPersonalDataManager::GetProfileSaveStrikeDatabase() const {
  return &inmemory_profile_save_strike_database_;
}

const AutofillProfileUpdateStrikeDatabase*
TestPersonalDataManager::GetProfileUpdateStrikeDatabase() const {
  return &inmemory_profile_update_strike_database_;
}

void TestPersonalDataManager::ClearProfiles() {
  synced_local_profiles_.clear();
  account_profiles_.clear();
}

void TestPersonalDataManager::ClearCreditCards() {
  local_credit_cards_.clear();
  server_credit_cards_.clear();
}

void TestPersonalDataManager::ClearCloudTokenData() {
  server_credit_card_cloud_token_data_.clear();
}

void TestPersonalDataManager::ClearCreditCardOfferData() {
  autofill_offer_data_.clear();
}

void TestPersonalDataManager::AddServerCreditCard(
    const CreditCard& credit_card) {
  std::unique_ptr<CreditCard> server_credit_card =
      std::make_unique<CreditCard>(credit_card);
  server_credit_cards_.push_back(std::move(server_credit_card));
  NotifyPersonalDataObserver();
}

void TestPersonalDataManager::AddCloudTokenData(
    const CreditCardCloudTokenData& cloud_token_data) {
  std::unique_ptr<CreditCardCloudTokenData> data =
      std::make_unique<CreditCardCloudTokenData>(cloud_token_data);
  server_credit_card_cloud_token_data_.push_back(std::move(data));
  NotifyPersonalDataObserver();
}

void TestPersonalDataManager::AddAutofillOfferData(
    const AutofillOfferData& offer_data) {
  std::unique_ptr<AutofillOfferData> data =
      std::make_unique<AutofillOfferData>(offer_data);
  autofill_offer_data_.emplace_back(std::move(data));
  NotifyPersonalDataObserver();
}

void TestPersonalDataManager::AddCardArtImage(const GURL& url,
                                              const gfx::Image& image) {
  credit_card_art_images_[url] = std::make_unique<gfx::Image>(image);
  NotifyPersonalDataObserver();
}

void TestPersonalDataManager::SetNicknameForCardWithGUID(
    const char* guid,
    const std::string& nickname) {
  for (auto& card : local_credit_cards_) {
    if (card->guid() == guid) {
      card->SetNickname(base::ASCIIToUTF16(nickname));
    }
  }
  for (auto& card : server_credit_cards_) {
    if (card->guid() == guid) {
      card->SetNickname(base::ASCIIToUTF16(nickname));
    }
  }
  NotifyPersonalDataObserver();
}

}  // namespace autofill
