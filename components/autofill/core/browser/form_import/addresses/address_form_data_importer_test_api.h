// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_ADDRESSES_ADDRESS_FORM_DATA_IMPORTER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_ADDRESSES_ADDRESS_FORM_DATA_IMPORTER_TEST_API_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_import/addresses/address_form_data_importer.h"
#include "components/autofill/core/browser/form_import/addresses/autofill_profile_import_process.h"
#include "components/autofill/core/browser/form_structure.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace autofill {

class AddressFormDataImporterTestApi {
 public:
  using ExtractedAddressProfile =
      AddressFormDataImporter::ExtractedAddressProfile;

  explicit AddressFormDataImporterTestApi(AddressFormDataImporter* address_fdi)
      : address_fdi_(*address_fdi) {}

  size_t ExtractAddressProfiles(
      const FormStructure& form,
      std::vector<ExtractedAddressProfile>* extracted_address_profiles) {
    return address_fdi_->ExtractAddressProfiles(form,
                                                extracted_address_profiles);
  }

  base::flat_set<std::string> ExtractGUIDsOfProfilesWithoutManualEdits(
      const FormStructure& submitted_form) const {
    return address_fdi_->ExtractGUIDsOfProfilesWithoutManualEdits(
        submitted_form);
  }

  bool ProcessExtractedAddressProfiles(
      const std::vector<ExtractedAddressProfile>& extracted_address_profiles,
      bool allow_prompt,
      ukm::SourceId ukm_source_id) {
    return address_fdi_->ProcessExtractedAddressProfiles(
        extracted_address_profiles, allow_prompt, ukm_source_id);
  }

  base::flat_map<FieldType, std::u16string> GetObservedFieldValues(
      base::span<const AutofillField* const> section_fields) {
    ProfileImportMetadata import_metadata;
    bool has_invalid_field_types = false;
    bool has_multiple_distinct_email_addresses = false;
    bool has_address_related_fields = false;
    return address_fdi_->GetAddressObservedFieldValues(
        section_fields, import_metadata, nullptr, has_invalid_field_types,
        has_multiple_distinct_email_addresses, has_address_related_fields);
  }

  bool HasInvalidFieldTypes(
      base::span<const AutofillField* const> section_fields) {
    ProfileImportMetadata import_metadata;
    bool has_invalid_field_types = false;
    bool has_multiple_distinct_email_addresses = false;
    bool has_address_related_fields = false;
    std::ignore = address_fdi_->GetAddressObservedFieldValues(
        section_fields, import_metadata, nullptr, has_invalid_field_types,
        has_multiple_distinct_email_addresses, has_address_related_fields);
    return has_invalid_field_types;
  }

 private:
  const raw_ref<AddressFormDataImporter> address_fdi_;
};

inline AddressFormDataImporterTestApi test_api(
    AddressFormDataImporter& address_fdi) {
  return AddressFormDataImporterTestApi(&address_fdi);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_ADDRESSES_ADDRESS_FORM_DATA_IMPORTER_TEST_API_H_
