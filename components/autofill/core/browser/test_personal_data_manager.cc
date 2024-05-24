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
  set_address_data_manager(
      std::make_unique<TestAddressDataManager>(app_locale()));
  set_payments_data_manager(
      std::make_unique<TestPaymentsDataManager>(app_locale()));
}

TestPersonalDataManager::~TestPersonalDataManager() = default;

void TestPersonalDataManager::set_address_data_manager(
    std::unique_ptr<TestAddressDataManager> address_data_manager) {
  address_data_manager_observation_.Reset();
  address_data_manager_ = std::move(address_data_manager);
  address_data_manager_observation_.Observe(address_data_manager_.get());
}

void TestPersonalDataManager::set_payments_data_manager(
    std::unique_ptr<TestPaymentsDataManager> payments_data_manager) {
  payments_data_manager_observation_.Reset();
  payments_data_manager_ = std::move(payments_data_manager);
  payments_data_manager_observation_.Observe(payments_data_manager_.get());
}

bool TestPersonalDataManager::IsDataLoaded() const {
  return true;
}

void TestPersonalDataManager::SetPrefService(PrefService* pref_service) {
  pref_service_ = pref_service;
  test_address_data_manager().SetPrefService(pref_service);
  test_payments_data_manager().SetPrefService(pref_service);
}

}  // namespace autofill
