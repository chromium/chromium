// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_PAYMENTS_PAYMENTS_FORM_DATA_IMPORTER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_PAYMENTS_PAYMENTS_FORM_DATA_IMPORTER_TEST_API_H_

#include "components/autofill/core/browser/form_import/payments/payments_form_data_importer.h"
#include "components/autofill/core/browser/payments/credit_card_save_manager.h"
#include "components/autofill/core/browser/payments/iban_save_manager.h"

namespace autofill::payments {

class PaymentsFormDataImporterTestApi {
 public:
  explicit PaymentsFormDataImporterTestApi(
      PaymentsFormDataImporter* payments_fdi)
      : payments_fdi_(*payments_fdi) {}

  std::optional<NonInteractivePaymentMethodType>
  payment_method_type_if_non_interactive_authentication_flow_completed() const {
    return payments_fdi_
        ->payment_method_type_if_non_interactive_authentication_flow_completed_;
  }

  void set_iban_save_manager(
      std::unique_ptr<IbanSaveManager> iban_save_manager) {
    payments_fdi_->iban_save_manager_ = std::move(iban_save_manager);
  }

  IbanSaveManager* iban_save_manager() {
    return payments_fdi_->iban_save_manager_.get();
  }

  std::optional<CreditCard> ExtractCreditCard(const FormStructure& form) {
    return payments_fdi_->ExtractCreditCard(form);
  }

  bool ProcessExtractedCreditCard(
      const FormStructure& submitted_form,
      const std::optional<CreditCard>& credit_card_import_candidate,
      bool is_credit_card_upstream_enabled,
      ukm::SourceId ukm_source_id) {
    return payments_fdi_->ProcessExtractedCreditCard(
        submitted_form, credit_card_import_candidate,
        is_credit_card_upstream_enabled, ukm_source_id);
  }

  void set_credit_card_save_manager(
      std::unique_ptr<CreditCardSaveManager> ccsm) {
    payments_fdi_->credit_card_save_manager_ = std::move(ccsm);
  }

  payments::PaymentsFormDataImporter::CreditCardImportType
  credit_card_import_type() const {
    return payments_fdi_->credit_card_import_type_;
  }

  void set_credit_card_import_type(
      payments::PaymentsFormDataImporter::CreditCardImportType
          credit_card_import_type) {
    payments_fdi_->credit_card_import_type_ = credit_card_import_type;
  }

 private:
  const raw_ref<PaymentsFormDataImporter> payments_fdi_;
};

inline PaymentsFormDataImporterTestApi test_api(
    PaymentsFormDataImporter& payments_fdi) {
  return PaymentsFormDataImporterTestApi(&payments_fdi);
}

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_PAYMENTS_PAYMENTS_FORM_DATA_IMPORTER_TEST_API_H_
