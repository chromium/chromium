// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_ADDRESSES_ADDRESS_FORM_DATA_IMPORTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_ADDRESSES_ADDRESS_FORM_DATA_IMPORTER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_import/addresses/autofill_profile_import_process.h"
#include "components/autofill/core/browser/form_import/form_data_importer_utils.h"
#include "url/gurl.h"

namespace autofill {

class AddressProfileSaveManager;
class AutofillClient;
class AutofillField;
class AutofillProfile;
class FormStructure;
class LogBuffer;
class PhoneCombineHelper;
class SourceId;

// Owned by `FormDataImporter`. Responsible for address-related form data
// importing functionality, including form extraction and processing.
class AddressFormDataImporter : public AddressDataManager::Observer {
 public:
  // Defines an extracted address profile, which is a candidate for address
  // profile import.
  struct ExtractedAddressProfile {
    ExtractedAddressProfile();
    ExtractedAddressProfile(const ExtractedAddressProfile& other);
    ~ExtractedAddressProfile();

    // The profile that was extracted from the form.
    AutofillProfile profile{i18n_model_definition::kLegacyHierarchyCountryCode};
    // The URL the profile was extracted from.
    GURL url;
    // Metadata about the import, used for metric collection in
    // ProfileImportProcess after the user's decision.
    ProfileImportMetadata import_metadata;
  };

  explicit AddressFormDataImporter(AutofillClient* client);
  AddressFormDataImporter(const AddressFormDataImporter&) = delete;
  AddressFormDataImporter& operator=(const AddressFormDataImporter&) = delete;
  ~AddressFormDataImporter() override;

  // AddressDataManager::Observer:
  void OnAddressDataChanged() override;

  // Attempts to construct `ExtractedAddressProfile` by extracting values
  // from the fields in the `form`'s sections. Extraction can fail if the
  // fields' values don't pass validation. Apart from complete address profiles,
  // partial profiles for silent updates are extracted. All are stored in
  // `extracted_form_data`'s `extracted_address_profiles`.
  // The function returns the number of _complete_ extracted profiles.
  size_t ExtractAddressProfiles(
      const FormStructure& form,
      std::vector<ExtractedAddressProfile>* extracted_address_profiles);

  // Processes the extracted address profiles. `extracted_address_profiles`
  // contains the addresses extracted from the form. `allow_prompt` denotes if a
  // prompt can be shown. Returns `true` if the import of a complete profile is
  // initiated.
  bool ProcessExtractedAddressProfiles(
      const std::vector<ExtractedAddressProfile>& extracted_address_profiles,
      bool allow_prompt,
      ukm::SourceId ukm_source_id);

  // Extracts the GUIDs of profiles used to autofill `submitted_form`, returning
  // an empty set if any field was manually edited.
  base::flat_set<std::string> ExtractGUIDsOfProfilesWithoutManualEdits(
      const FormStructure& submitted_form) const;

  AddressDataManager& address_data_manager();

  MultiStepImportMerger& multi_step_import_merger();

 private:
  friend class AddressFormDataImporterTestApi;

  // Iterates over `section_fields` and builds a map from field type to observed
  // value for that field type.
  base::flat_map<FieldType, std::u16string> GetAddressObservedFieldValues(
      base::span<const AutofillField* const> section_fields,
      ProfileImportMetadata& import_metadata,
      LogBuffer* import_log_buffer,
      bool& has_invalid_field_types,
      bool& has_multiple_distinct_email_addresses,
      bool& has_address_related_fields) const;

  // Helper method to construct an `AutofillProfile` out of observed values in
  // the form. Used during `ExtractAddressProfileFromSection()`.
  AutofillProfile ConstructProfileFromObservedValues(
      const base::flat_map<FieldType, std::u16string>& observed_values,
      LogBuffer* import_log_buffer,
      ProfileImportMetadata& import_metadata);

  // Helper method for `ExtractAddressProfiles` which only considers the fields
  // for the specified `section_fields`.
  bool ExtractAddressProfileFromSection(
      base::span<const AutofillField* const> section_fields,
      const GURL& source_url,
      mojom::SubmissionSource submission_source,
      std::vector<ExtractedAddressProfile>* extracted_address_profiles,
      LogBuffer* import_log_buffer);

  // Clears all setting-inaccessible values from `profile`.
  void RemoveInaccessibleProfileValues(AutofillProfile& profile);

  // If the `profile`'s country is not empty, complements it with
  // `AddressDataManager::GetDefaultCountryCodeForNewAddress()`, while logging
  // to the `import_log_buffer`.
  // Returns true if the country was complemented.
  bool ComplementCountry(AutofillProfile& profile,
                         LogBuffer* import_log_buffer);

  // Sets the `profile`'s PHONE_HOME_WHOLE_NUMBER to the `combined_phone`, if
  // possible. The phone number's region is deduced based on the profile's
  // country or alternatively the app locale.
  // Returns false if the provided `combined_phone` is invalid.
  bool SetPhoneNumber(AutofillProfile& profile,
                      const PhoneNumber::PhoneCombineHelper& combined_phone);

  base::ScopedObservation<AddressDataManager, AddressDataManager::Observer>
      address_data_manager_observation_{this};

  const raw_ref<AutofillClient> client_;

  // Enables importing from multi-step import flows.
  MultiStepImportMerger multistep_importer_;

  // Responsible for managing address profiles save flows.
  std::unique_ptr<AddressProfileSaveManager> address_profile_save_manager_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_ADDRESSES_ADDRESS_FORM_DATA_IMPORTER_H_
