// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_payments_data_manager.h"

namespace autofill {

TestPaymentsDataManager::TestPaymentsDataManager(const std::string& app_locale,
                                                 PersonalDataManager* pdm)
    : PaymentsDataManager(/*profile_database=*/nullptr,
                          /*account_database=*/nullptr,
                          /*image_fetcher=*/nullptr,
                          /*shared_storage_handler=*/nullptr,
                          app_locale,
                          pdm) {}

TestPaymentsDataManager::~TestPaymentsDataManager() = default;

void TestPaymentsDataManager::ClearCreditCards() {
  local_credit_cards_.clear();
  server_credit_cards_.clear();
}

void TestPaymentsDataManager::ClearCreditCardOfferData() {
  autofill_offer_data_.clear();
}

}  // namespace autofill
