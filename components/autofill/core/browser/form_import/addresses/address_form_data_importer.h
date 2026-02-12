// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_ADDRESSES_ADDRESS_FORM_DATA_IMPORTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_ADDRESSES_ADDRESS_FORM_DATA_IMPORTER_H_

#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_import/addresses/autofill_profile_import_process.h"
#include "url/gurl.h"

namespace autofill {

class AddressDataManager;
class AutofillClient;
class AutofillField;
class AutofillProfile;
class LogBuffer;

// Owned by `FormDataImporter`. Responsible for address-related form data
// importing functionality, including form extraction and processing.
class AddressFormDataImporter {
 public:
  explicit AddressFormDataImporter(AutofillClient* client);
  AddressFormDataImporter(const AddressFormDataImporter&) = delete;
  AddressFormDataImporter& operator=(const AddressFormDataImporter&) = delete;
  virtual ~AddressFormDataImporter();

  AddressDataManager& address_data_manager();

 private:
  friend class AddressFormDataImporterTestApi;
  // TODO(crbug.com/481379161): Remove FormDataImporter and
  //    FormDataImporterTestApi as friend classes once the FDI->AddressFDI
  //    migration is complete. This is very much not ideal and temporary, but
  //    the alternative is having most functions be public until the last
  //    second, which probably carries slightly higher risk.
  friend class FormDataImporter;
  friend class FormDataImporterTestApi;

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

  // Iterates over `section_fields` and builds a map from field type to observed
  // value for that field type.
  base::flat_map<FieldType, std::u16string> GetAddressObservedFieldValues(
      base::span<const AutofillField* const> section_fields,
      ProfileImportMetadata& import_metadata,
      LogBuffer* import_log_buffer,
      bool& has_invalid_field_types,
      bool& has_multiple_distinct_email_addresses,
      bool& has_address_related_fields) const;

  // Clears all setting-inaccessible values from `profile`.
  void RemoveInaccessibleProfileValues(AutofillProfile& profile);

  const raw_ref<AutofillClient> client_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_ADDRESSES_ADDRESS_FORM_DATA_IMPORTER_H_
