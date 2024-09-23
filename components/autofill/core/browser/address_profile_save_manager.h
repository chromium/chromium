// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_PROFILE_SAVE_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_PROFILE_SAVE_MANAGER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_profile_import_process.h"

namespace autofill {

class AddressDataManager;
class AutofillProfile;

// Manages logic for saving address profiles to the database. Owned by
// FormDataImporter.
class AddressProfileSaveManager {
 public:
  explicit AddressProfileSaveManager(AutofillClient* client);
  AddressProfileSaveManager(const AddressProfileSaveManager&) = delete;
  AddressProfileSaveManager& operator=(const AddressProfileSaveManager&) =
      delete;

  virtual ~AddressProfileSaveManager();

  // This method initiates the import process that is started when an importable
  // `profile` is observed in a form submission on `url`. Depending on the
  // scenario, the method will have no effect if `profile` resembles an already
  // existing profile. If the import corresponds to a new profile, or to a
  // change of an existing profile that must be confirmed by the user, a UI
  // prompt will be initiated. At the end of the process, metrics will be
  // recorded.
  // |allow_only_silent_updates| allows only for silent updates of profiles
  // that have either a structured name or address or both but do not fulfill
  // the import requirements.
  // |import_metadata| is passed through, to collect metrics based on the
  // profile import decision.
  void ImportProfileFromForm(const AutofillProfile& profile,
                             const std::string& app_locale,
                             const GURL& url,
                             bool allow_only_silent_updates,
                             ProfileImportMetadata import_metadata);

 protected:
  // Initiates showing the prompt to the user.
  // This function is virtual to be mocked in tests.
  virtual void OfferSavePrompt(
      std::unique_ptr<ProfileImportProcess> import_process);

  // Clears the pending import. This method can be overloaded to store the
  // history of import processes for testing purposes.
  virtual void ClearPendingImport(
      std::unique_ptr<ProfileImportProcess> import_process);

  // Called after the user interaction with the UI is done.
  void OnUserDecision(std::unique_ptr<ProfileImportProcess> import_process,
                      AutofillClient::AddressPromptUserDecision decision,
                      base::optional_ref<const AutofillProfile> edited_profile);

  AddressDataManager& address_data_manager();

 private:
  // Called to initiate the actual storing of a profile.
  // Verifies that the profile was actually imported.
  void FinalizeProfileImport(
      std::unique_ptr<ProfileImportProcess> import_process);

  // Called to make the final decision if the UI should be shown, or if the
  // import process should be continued silently.
  void MaybeOfferSavePrompt(
      std::unique_ptr<ProfileImportProcess> import_process);

  // Increases or resets the strike count depending on the user decision for
  // the corresponding prompt type.
  void AdjustNewProfileStrikes(ProfileImportProcess& import_process);
  void AdjustUpdateProfileStrikes(ProfileImportProcess& import_process);
  void AdjustMigrateProfileStrikes(ProfileImportProcess& import_process);

  // The client must outlive the instance of this class
  const raw_ref<AutofillClient> client_;

  base::WeakPtrFactory<AddressProfileSaveManager> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_PROFILE_SAVE_MANAGER_H_
