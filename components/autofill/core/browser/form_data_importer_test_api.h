// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_DATA_IMPORTER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_DATA_IMPORTER_TEST_API_H_

#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/payments/credit_card_save_manager.h"

namespace autofill {

class FormDataImporterTestApi {
 public:
  using AddressProfileImportCandidate =
      FormDataImporter::AddressProfileImportCandidate;
  using ExtractedFormData = FormDataImporter::ExtractedFormData;

  explicit FormDataImporterTestApi(FormDataImporter* fdi) : fdi_(*fdi) {}

  void set_credit_card_save_manager(
      std::unique_ptr<CreditCardSaveManager> ccsm) {
    fdi_->credit_card_save_manager_ = std::move(ccsm);
  }

  void set_virtual_card_enrollment_manager(
      std::unique_ptr<VirtualCardEnrollmentManager> vcem) {
    fdi_->virtual_card_enrollment_manager_ = std::move(vcem);
  }

  void set_iban_save_manager(
      std::unique_ptr<IbanSaveManager> iban_save_manager) {
    fdi_->iban_save_manager_ = std::move(iban_save_manager);
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  void set_local_card_migration_manager(
      std::unique_ptr<LocalCardMigrationManager> local_card_migration_manager) {
    fdi_->local_card_migration_manager_ =
        std::move(local_card_migration_manager);
  }
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

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

  absl::optional<CreditCard> ExtractCreditCard(const FormStructure& form) {
    return fdi_->ExtractCreditCard(form);
  }

  size_t ExtractAddressProfiles(const FormStructure& form,
                                std::vector<AddressProfileImportCandidate>*
                                    address_profile_import_candidates) {
    return fdi_->ExtractAddressProfiles(form,
                                        address_profile_import_candidates);
  }

  bool ProcessAddressProfileImportCandidates(
      const std::vector<AddressProfileImportCandidate>&
          address_profile_import_candidates,
      bool allow_prompt = true) {
    return fdi_->ProcessAddressProfileImportCandidates(
        address_profile_import_candidates, allow_prompt);
  }

  ExtractedFormData ExtractFormData(const FormStructure& form,
                                    bool profile_autofill_enabled,
                                    bool payment_methods_autofill_enabled) {
    return fdi_->ExtractFormData(form, profile_autofill_enabled,
                                 payment_methods_autofill_enabled);
  }

  bool ProcessExtractedCreditCard(
      const FormStructure& submitted_form,
      const absl::optional<CreditCard>& credit_card_import_candidate,
      bool payment_methods_autofill_enabled,
      bool is_credit_card_upstream_enabled) {
    return fdi_->ProcessExtractedCreditCard(
        submitted_form, credit_card_import_candidate,
        payment_methods_autofill_enabled, is_credit_card_upstream_enabled);
  }

  absl::optional<int64_t> fetched_card_instrument_id() {
    return fdi_->fetched_card_instrument_id_;
  }

 private:
  const raw_ref<FormDataImporter> fdi_;
};

inline FormDataImporterTestApi test_api(FormDataImporter& fdi) {
  return FormDataImporterTestApi(&fdi);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_DATA_IMPORTER_TEST_API_H_
