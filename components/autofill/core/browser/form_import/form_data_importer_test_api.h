// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_FORM_DATA_IMPORTER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_FORM_DATA_IMPORTER_TEST_API_H_

#include <string>

#include "base/containers/flat_map.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_import/form_data_importer.h"
#include "components/autofill/core/browser/payments/credit_card_save_manager.h"
#include "components/autofill/core/browser/payments/iban_save_manager.h"

namespace autofill {

class FormDataImporterTestApi {
 public:
  using ExtractedAddressProfile = FormDataImporter::ExtractedAddressProfile;
  using ExtractedFormData = FormDataImporter::ExtractedFormData;

  explicit FormDataImporterTestApi(FormDataImporter* fdi) : fdi_(*fdi) {}

  std::optional<NonInteractivePaymentMethodType>
  payment_method_type_if_non_interactive_authentication_flow_completed() const {
    return fdi_
        ->payment_method_type_if_non_interactive_authentication_flow_completed_;
  }

  void set_credit_card_save_manager(
      std::unique_ptr<CreditCardSaveManager> ccsm) {
    fdi_->credit_card_save_manager_ = std::move(ccsm);
  }

  void set_iban_save_manager(
      std::unique_ptr<IbanSaveManager> iban_save_manager) {
    fdi_->iban_save_manager_ = std::move(iban_save_manager);
  }

  FormDataImporter::CreditCardImportType credit_card_import_type() const {
    return fdi_->credit_card_import_type_;
  }

  void set_credit_card_import_type(
      FormDataImporter::CreditCardImportType credit_card_import_type) {
    fdi_->credit_card_import_type_ = credit_card_import_type;
  }

  IbanSaveManager* iban_save_manager() {
    return fdi_->iban_save_manager_.get();
  }

  std::optional<CreditCard> ExtractCreditCard(const FormStructure& form) {
    return fdi_->ExtractCreditCard(form);
  }

  size_t ExtractAddressProfiles(
      const FormStructure& form,
      std::vector<ExtractedAddressProfile>* extracted_address_profiles) {
    return fdi_->ExtractAddressProfiles(form, extracted_address_profiles);
  }

  base::flat_set<std::string> ExtractGUIDsOfProfilesWithoutManualEdits(
      const FormStructure& submitted_form) const {
    return fdi_->ExtractGUIDsOfProfilesWithoutManualEdits(submitted_form);
  }

  base::flat_map<FieldType, std::u16string> GetObservedFieldValues(
      base::span<const AutofillField* const> section_fields) {
    ProfileImportMetadata import_metadata;
    bool has_invalid_field_types = false;
    bool has_multiple_distinct_email_addresses = false;
    bool has_address_related_fields = false;
    return fdi_->GetAddressObservedFieldValues(
        section_fields, import_metadata, nullptr, has_invalid_field_types,
        has_multiple_distinct_email_addresses, has_address_related_fields);
  }

  bool HasInvalidFieldTypes(
      base::span<const AutofillField* const> section_fields) {
    ProfileImportMetadata import_metadata;
    bool has_invalid_field_types = false;
    bool has_multiple_distinct_email_addresses = false;
    bool has_address_related_fields = false;
    std::ignore = fdi_->GetAddressObservedFieldValues(
        section_fields, import_metadata, nullptr, has_invalid_field_types,
        has_multiple_distinct_email_addresses, has_address_related_fields);
    return has_invalid_field_types;
  }

  bool ProcessExtractedAddressProfiles(
      const std::vector<ExtractedAddressProfile>& extracted_address_profiles,
      bool allow_prompt,
      ukm::SourceId ukm_source_id) {
    return fdi_->ProcessExtractedAddressProfiles(extracted_address_profiles,
                                                 allow_prompt, ukm_source_id);
  }

  ExtractedFormData ExtractFormData(const FormStructure& form,
                                    bool profile_autofill_enabled,
                                    bool payment_methods_autofill_enabled) {
    return fdi_->ExtractFormData(form, profile_autofill_enabled,
                                 payment_methods_autofill_enabled);
  }

  bool ProcessExtractedCreditCard(
      const FormStructure& submitted_form,
      const std::optional<CreditCard>& credit_card_import_candidate,
      bool is_credit_card_upstream_enabled,
      ukm::SourceId ukm_source_id) {
    return fdi_->ProcessExtractedCreditCard(
        submitted_form, credit_card_import_candidate,
        is_credit_card_upstream_enabled, ukm_source_id);
  }

  void ImportAndProcessFormData(const FormStructure& submitted_form,
                                bool profile_autofill_enabled,
                                bool payment_methods_autofill_enabled,
                                ukm::SourceId ukm_source_id) {
    fdi_->ImportAndProcessFormData(submitted_form, profile_autofill_enabled,
                                   payment_methods_autofill_enabled,
                                   ukm_source_id);
  }

 private:
  const raw_ref<FormDataImporter> fdi_;
};

inline FormDataImporterTestApi test_api(FormDataImporter& fdi) {
  return FormDataImporterTestApi(&fdi);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_FORM_DATA_IMPORTER_TEST_API_H_
