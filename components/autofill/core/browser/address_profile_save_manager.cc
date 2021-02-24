// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_profile_save_manager.h"

#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

AddressProfileSaveManager::AddressProfileSaveManager(
    AutofillClient* client,
    PersonalDataManager* personal_data_manager)
    : client_(client), personal_data_manager_(personal_data_manager) {}

AddressProfileSaveManager::~AddressProfileSaveManager() = default;

void AddressProfileSaveManager::SaveProfile(const AutofillProfile& profile) {
  if (!personal_data_manager_)
    return;

  if (base::FeatureList::IsEnabled(
          features::kAutofillAddressProfileSavePrompt)) {
    client_->ConfirmSaveAddressProfile(
        profile,
        base::BindOnce(&AddressProfileSaveManager::SaveProfilePromptCallback,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  SaveProfileInternal(profile);
}

void AddressProfileSaveManager::SaveProfileInternal(
    const AutofillProfile& profile) {
  personal_data_manager_->SaveImportedProfile(profile);
}

void AddressProfileSaveManager::SaveProfilePromptCallback(
    AutofillClient::SaveAddressProfileOfferUserDecision user_decision,
    AutofillProfile profile) {
  switch (user_decision) {
    case AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted:
    case AutofillClient::SaveAddressProfileOfferUserDecision::kEdited:
      personal_data_manager_->SaveImportedProfile(profile);
      break;
    case AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined:
    case AutofillClient::SaveAddressProfileOfferUserDecision::kIgnored:
      break;
  }
}

}  // namespace autofill
