// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/upi_vpa_save_manager.h"

#include "base/logging.h"

namespace autofill {

UpiVpaSaveManager::UpiVpaSaveManager(PersonalDataManager* personal_data_manager)
    : personal_data_manager_(personal_data_manager) {}

void UpiVpaSaveManager::OfferLocalSave(const std::string& upi_id) {
  // TODO(crbug.com/986289): Ask user with a prompt before saving.

  if (personal_data_manager_)
    personal_data_manager_->AddVPA(upi_id);
}

}  // namespace autofill
