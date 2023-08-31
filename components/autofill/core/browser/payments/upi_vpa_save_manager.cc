// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/upi_vpa_save_manager.h"

#include "build/build_config.h"

namespace autofill {

UpiVpaSaveManager::UpiVpaSaveManager(AutofillClient* client,
                                     PersonalDataManager* personal_data_manager)
    : client_(client), personal_data_manager_(personal_data_manager) {}

UpiVpaSaveManager::~UpiVpaSaveManager() = default;

void UpiVpaSaveManager::OfferLocalSave(const std::string& upi_id) {
  if (!personal_data_manager_)
    return;

  personal_data_manager_->AddUpiId(upi_id);
}

}  // namespace autofill
