// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/valuables/test_valuables_data_manager.h"

namespace autofill {

TestValuablesDataManager::TestValuablesDataManager()
    : ValuablesDataManager(/*webdata_service=*/nullptr) {}
TestValuablesDataManager::~TestValuablesDataManager() = default;

base::span<const LoyaltyCard> TestValuablesDataManager::GetLoyaltyCards()
    const {
  return {};
}

}  // namespace autofill
