// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_PROFILES_ADDRESS_PROFILE_SAVE_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_PROFILES_ADDRESS_PROFILE_SAVE_MANAGER_H_

#include <string>

namespace autofill {

class AutofillProfile;
class PersonalDataManager;

// Manages logic for saving address profiles to the database. Owned by
// FormDataImporter.
class AddressProfileSaveManager {
 public:
  explicit AddressProfileSaveManager(
      PersonalDataManager* personal_data_manager);
  AddressProfileSaveManager(const AddressProfileSaveManager&) = delete;
  AddressProfileSaveManager& operator=(const AddressProfileSaveManager&) =
      delete;
  virtual ~AddressProfileSaveManager();

  // Saves `imported_profile` using the `personal_data_manager_`. Returns the
  // guid of the new or updated profile, or the empty string if no profile was
  // saved.
  std::string SaveProfile(const AutofillProfile& imported_profile);

 private:
  // The personal data manager, used to save and load personal data to/from the
  // web database.
  PersonalDataManager* const personal_data_manager_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_PROFILES_ADDRESS_PROFILE_SAVE_MANAGER_H_
