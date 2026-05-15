// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_FORM_DATA_IMPORTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_FORM_DATA_IMPORTER_H_

#include <optional>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/form_import/addresses/address_form_data_importer.h"
#include "components/autofill/core/browser/form_import/form_data_importer_utils.h"
#include "components/autofill/core/browser/form_import/payments/payments_form_data_importer.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/signatures.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/history_types.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace history {
class HistoryService;
}  // namespace history

namespace autofill {

class AutofillClient;
class PaymentsDataManager;
class SourceId;

// Manages logic for importing address profiles and credit card information from
// web forms into the user's Autofill profile via the `AddressDataManager` and
// the `PaymentsDataManager`. Owned by `AutofillClient` implementations.
class FormDataImporter : public history::HistoryServiceObserver {
 public:
  // The parameters should outlive the FormDataImporter.
  FormDataImporter(AutofillClient* client,
                   history::HistoryService* history_service);

  FormDataImporter(const FormDataImporter&) = delete;
  FormDataImporter& operator=(const FormDataImporter&) = delete;

  ~FormDataImporter() override;

  // Imports the form data submitted by the user. If a new credit card was
  // detected and `payment_methods_autofill_enabled` is set to `true`, also
  // begins the process to offer local or upload credit card save.
  void ImportAndProcessFormData(const FormStructure& submitted_form,
                                bool profile_autofill_enabled,
                                bool payment_methods_autofill_enabled,
                                ukm::SourceId ukm_source_id);

  // history::HistoryServiceObserver
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;

  // See `FormAssociator::GetFormAssociations()`.
  FormStructure::FormAssociations GetFormAssociations(
      FormSignature form_signature) const {
    return form_associator_.GetFormAssociations(form_signature);
  }

  // Gets the AddressFormDataImporter owned by `this`.
  AddressFormDataImporter& GetAddressFormDataImporter();

  // Gets the payments::PaymentsFormDataImporter owned by `this`.
  payments::PaymentsFormDataImporter& GetPaymentsFormDataImporter();

 private:
  // Defines data extracted from the form.
  struct ExtractedFormData {
    ExtractedFormData();
    ExtractedFormData(const ExtractedFormData& extracted_form_data);
    ExtractedFormData& operator=(const ExtractedFormData& extracted_form_data);
    ~ExtractedFormData();

    // Credit card extracted from the form, which is a candidate for importing.
    // This credit card will be present after extraction if the form contained a
    // valid credit card, and the preconditions for extracting the credit card
    // were met. See `ExtractCreditCard()` for details on when
    // the preconditions are met for extracting a credit card from a form.
    std::optional<CreditCard> extracted_credit_card;
    // List of address profiles extracted from the form, which are candidates
    // for importing. The list is empty if none of the address profile fulfill
    // import requirements.
    std::vector<AddressFormDataImporter::ExtractedAddressProfile>
        extracted_address_profiles;
    // IBAN extracted from the form, which is a candidate for importing. Present
    // if an IBAN is found in the form.
    std::optional<Iban> extracted_iban;
  };

  // Scans the given `form` for extractable Autofill data.
  ExtractedFormData ExtractFormData(const FormStructure& form,
                                    bool profile_autofill_enabled,
                                    bool payment_methods_autofill_enabled);

  PaymentsDataManager& payments_data_manager();

  // The associated autofill client.
  const raw_ref<AutofillClient> client_;

  base::ScopedObservation<history::HistoryService, HistoryServiceObserver>
      history_service_observation_{this};

  // Enables associating recently submitted forms with each other.
  FormAssociator form_associator_;

  // FormDataImporter to handle address-related functionality.
  AddressFormDataImporter address_form_data_importer_;

  // FormDataImporter to handle payments-related functionality.
  payments::PaymentsFormDataImporter payments_form_data_importer_;

  friend class FormDataImporterTestApi;

  // TODO(crbug.com/481379161): Remove `payments::PaymentsFormDataImporter` as a
  //    friend class once the FDI->PaymentsFDI migration is complete. This is
  //    very much not ideal and temporary, but the alternative is temporarily
  //    passing in class variables as parameters until the last second, which
  //    probably carries slightly higher risk.
  friend class payments::PaymentsFormDataImporter;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_FORM_DATA_IMPORTER_H_
