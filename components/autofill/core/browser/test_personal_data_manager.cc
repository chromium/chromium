// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_personal_data_manager.h"

#include <memory>

#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/test_address_data_manager.h"
#include "components/autofill/core/browser/test_payments_data_manager.h"

namespace autofill {

TestPersonalDataManager::TestPersonalDataManager()
    : PersonalDataManager("en-US", "US") {
  address_data_manager_ = std::make_unique<TestAddressDataManager>(
      base::BindRepeating(&PersonalDataManager::NotifyPersonalDataObserver,
                          base::Unretained(this)));
  payments_data_manager_ =
      std::make_unique<TestPaymentsDataManager>(app_locale(), this);
}

TestPersonalDataManager::~TestPersonalDataManager() = default;

bool TestPersonalDataManager::IsPaymentsWalletSyncTransportEnabled() const {
  if (payments_wallet_sync_transport_enabled_.has_value()) {
    return *payments_wallet_sync_transport_enabled_;
  }
  return PersonalDataManager::IsPaymentsWalletSyncTransportEnabled();
}

std::string TestPersonalDataManager::SaveImportedCreditCard(
    const CreditCard& imported_credit_card) {
  num_times_save_imported_credit_card_called_++;
  AddCreditCard(imported_credit_card);
  return imported_credit_card.guid();
}

bool TestPersonalDataManager::IsEligibleForAddressAccountStorage() const {
  return eligible_for_account_storage_.has_value()
             ? *eligible_for_account_storage_
             : PersonalDataManager::IsEligibleForAddressAccountStorage();
}

const std::string& TestPersonalDataManager::GetDefaultCountryCodeForNewAddress()
    const {
  if (default_country_code_.empty())
    return PersonalDataManager::GetDefaultCountryCodeForNewAddress();

  return default_country_code_;
}

bool TestPersonalDataManager::IsAutofillWalletImportEnabled() const {
  // Return the value of autofill_wallet_import_enabled_ if it has been set,
  // otherwise fall back to the normal behavior of checking the pref_service.
  if (autofill_wallet_import_enabled_.has_value())
    return autofill_wallet_import_enabled_.value();
  return PersonalDataManager::IsAutofillWalletImportEnabled();
}

bool TestPersonalDataManager::ShouldSuggestServerPaymentMethods() const {
  return payments_data_manager().IsAutofillPaymentMethodsEnabled() &&
         IsAutofillWalletImportEnabled();
}

void TestPersonalDataManager::ClearAllLocalData() {
  ClearProfiles();
  payments_data_manager_->local_credit_cards_.clear();
}

bool TestPersonalDataManager::IsDataLoaded() const {
  return true;
}

bool TestPersonalDataManager::IsSyncFeatureEnabledForPaymentsServerMetrics()
    const {
  return false;
}

CoreAccountInfo TestPersonalDataManager::GetAccountInfoForPaymentsServer()
    const {
  return account_info_;
}

bool TestPersonalDataManager::IsPaymentMethodsMandatoryReauthEnabled() {
  if (payment_methods_mandatory_reauth_enabled_.has_value()) {
    return payment_methods_mandatory_reauth_enabled_.value();
  }
  return PersonalDataManager::IsPaymentMethodsMandatoryReauthEnabled();
}

void TestPersonalDataManager::SetPaymentMethodsMandatoryReauthEnabled(
    bool enabled) {
  payment_methods_mandatory_reauth_enabled_ = enabled;
  PersonalDataManager::SetPaymentMethodsMandatoryReauthEnabled(enabled);
}

bool TestPersonalDataManager::IsPaymentCvcStorageEnabled() {
  if (payments_cvc_storage_enabled_.has_value()) {
    return payments_cvc_storage_enabled_.value();
  }
  return PersonalDataManager::IsPaymentCvcStorageEnabled();
}

void TestPersonalDataManager::SetPrefService(PrefService* pref_service) {
  pref_service_ = pref_service;
  test_address_data_manager().SetPrefService(pref_service);
  test_payments_data_manager().SetPrefService(pref_service);
}

void TestPersonalDataManager::ClearProfiles() {
  test_address_data_manager().ClearProfiles();
}

void TestPersonalDataManager::AddServerCreditCard(
    const CreditCard& credit_card) {
  std::unique_ptr<CreditCard> server_credit_card =
      std::make_unique<CreditCard>(credit_card);
  payments_data_manager_->server_credit_cards_.push_back(
      std::move(server_credit_card));
  NotifyPersonalDataObserver();
}

void TestPersonalDataManager::AddCloudTokenData(
    const CreditCardCloudTokenData& cloud_token_data) {
  std::unique_ptr<CreditCardCloudTokenData> data =
      std::make_unique<CreditCardCloudTokenData>(cloud_token_data);
  payments_data_manager_->server_credit_card_cloud_token_data_.push_back(
      std::move(data));
  NotifyPersonalDataObserver();
}

void TestPersonalDataManager::AddAutofillOfferData(
    const AutofillOfferData& offer_data) {
  std::unique_ptr<AutofillOfferData> data =
      std::make_unique<AutofillOfferData>(offer_data);
  payments_data_manager_->autofill_offer_data_.emplace_back(std::move(data));
  NotifyPersonalDataObserver();
}

void TestPersonalDataManager::AddServerIban(const Iban& iban) {
  CHECK(iban.value().empty());
  payments_data_manager_->server_ibans_.push_back(std::make_unique<Iban>(iban));
  NotifyPersonalDataObserver();
}

void TestPersonalDataManager::AddCardArtImage(const GURL& url,
                                              const gfx::Image& image) {
  payments_data_manager_->credit_card_art_images_[url] =
      std::make_unique<gfx::Image>(image);
  NotifyPersonalDataObserver();
}

void TestPersonalDataManager::AddVirtualCardUsageData(
    const VirtualCardUsageData& usage_data) {
  payments_data_manager_->autofill_virtual_card_usage_data_.push_back(
      std::make_unique<VirtualCardUsageData>(usage_data));
  NotifyPersonalDataObserver();
}

void TestPersonalDataManager::SetNicknameForCardWithGUID(
    std::string_view guid,
    std::string_view nickname) {
  for (auto& card : payments_data_manager_->local_credit_cards_) {
    if (card->guid() == guid) {
      card->SetNickname(base::ASCIIToUTF16(nickname));
    }
  }
  for (auto& card : payments_data_manager_->server_credit_cards_) {
    if (card->guid() == guid) {
      card->SetNickname(base::ASCIIToUTF16(nickname));
    }
  }
  NotifyPersonalDataObserver();
}

}  // namespace autofill
