// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_personal_data_manager.h"

#include <memory>

#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/test_address_data_manager.h"
#include "components/autofill/core/browser/test_payments_data_manager.h"

namespace autofill {

TestPersonalDataManager::TestPersonalDataManager()
    : PersonalDataManager(
          /*profile_database=*/nullptr,
          /*account_database=*/nullptr,
          /*pref_service=*/nullptr,
          /*local_state=*/nullptr,
          /*identity_manager=*/nullptr,
          /*history_service=*/nullptr,
          /*sync_service=*/nullptr,
          /*strike_database=*/nullptr,
          /*image_fetcher=*/nullptr,
          /*shared_storage_handler=*/nullptr,
          "en-US",
          "US") {
  auto notify_observers = base::BindRepeating(
      &PersonalDataManager::NotifyPersonalDataObserver, base::Unretained(this));
  address_data_manager_ =
      std::make_unique<TestAddressDataManager>(notify_observers, app_locale());
  payments_data_manager_ =
      std::make_unique<TestPaymentsDataManager>(notify_observers, app_locale());
}

TestPersonalDataManager::~TestPersonalDataManager() = default;

void TestPersonalDataManager::ClearAllLocalData() {
  test_address_data_manager().ClearProfiles();
  test_payments_data_manager().ClearAllLocalData();
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
  test_payments_data_manager().AddServerCreditCard(credit_card);
}

void TestPersonalDataManager::AddAutofillOfferData(
    const AutofillOfferData& offer_data) {
  test_payments_data_manager().AddAutofillOfferData(offer_data);
}

void TestPersonalDataManager::SetAutofillPaymentMethodsEnabled(
    bool autofill_payment_methods_enabled) {
  test_payments_data_manager().SetAutofillPaymentMethodsEnabled(
      autofill_payment_methods_enabled);
}

void TestPersonalDataManager::SetAutofillProfileEnabled(
    bool autofill_profile_enabled) {
  test_address_data_manager().SetAutofillProfileEnabled(
      autofill_profile_enabled);
}

void TestPersonalDataManager::SetAutofillWalletImportEnabled(
    bool autofill_wallet_import_enabled) {
  test_payments_data_manager().SetAutofillWalletImportEnabled(
      autofill_wallet_import_enabled);
}

}  // namespace autofill
