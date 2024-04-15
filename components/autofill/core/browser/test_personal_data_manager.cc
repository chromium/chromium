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
  auto notify_observers = base::BindRepeating(
      &PersonalDataManager::NotifyPersonalDataObserver, base::Unretained(this));
  address_data_manager_ =
      std::make_unique<TestAddressDataManager>(notify_observers, app_locale());
  payments_data_manager_ =
      std::make_unique<TestPaymentsDataManager>(notify_observers, app_locale());
}

TestPersonalDataManager::~TestPersonalDataManager() = default;

void TestPersonalDataManager::ClearAllLocalData() {
  ClearProfiles();
  payments_data_manager_->local_credit_cards_.clear();
  payments_data_manager_->local_ibans_.clear();
}

bool TestPersonalDataManager::IsDataLoaded() const {
  return true;
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
