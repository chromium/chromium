// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_PROFILE_SAVE_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_PROFILE_SAVE_MANAGER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_client.h"

namespace autofill {

class AutofillProfile;
class PersonalDataManager;

// Manages logic for saving address profiles to the database. Owned by
// FormDataImporter.
class AddressProfileSaveManager {
 public:
  // The parameters should outlive the AddressProfileSaveManager.
  AddressProfileSaveManager(AutofillClient* client,
                            PersonalDataManager* personal_data_manager);
  AddressProfileSaveManager(const AddressProfileSaveManager&) = delete;
  AddressProfileSaveManager& operator=(const AddressProfileSaveManager&) =
      delete;
  virtual ~AddressProfileSaveManager();

  // Saves `profile` using the `personal_data_manager_`.
  void SaveProfile(const AutofillProfile& profile);

 private:
  void SaveProfilePromptCallback(
      AutofillClient::SaveAddressProfileOfferUserDecision user_decision,
      AutofillProfile profile);
  void SaveProfileInternal(const AutofillProfile& profile);

  AutofillClient* const client_;

  // The personal data manager, used to save and load personal data to/from the
  // web database.
  PersonalDataManager* const personal_data_manager_;

  base::WeakPtrFactory<AddressProfileSaveManager> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_PROFILE_SAVE_MANAGER_H_
