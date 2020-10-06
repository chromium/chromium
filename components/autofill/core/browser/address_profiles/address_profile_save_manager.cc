// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_profiles/address_profile_save_manager.h"

#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/personal_data_manager.h"

namespace autofill {

AddressProfileSaveManager::AddressProfileSaveManager(
    PersonalDataManager* personal_data_manager)
    : personal_data_manager_(personal_data_manager) {}

AddressProfileSaveManager::~AddressProfileSaveManager() = default;

std::string AddressProfileSaveManager::SaveProfile(
    const AutofillProfile& profile) {
  return personal_data_manager_
             ? personal_data_manager_->SaveImportedProfile(profile)
             : std::string();
}

}  // namespace autofill
