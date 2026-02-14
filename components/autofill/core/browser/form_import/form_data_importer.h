// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_FORM_DATA_IMPORTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_FORM_DATA_IMPORTER_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/form_import/addresses/address_form_data_importer.h"
#include "components/autofill/core/browser/form_import/addresses/autofill_profile_import_process.h"
#include "components/autofill/core/browser/form_import/payments/payments_form_data_importer.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/history/core/browser/history_service_observer.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace history {
class HistoryService;
}  // namespace history

namespace autofill {

class AddressProfileSaveManager;
class AutofillClient;
class CreditCardSaveManager;
class IbanSaveManager;
class PaymentsDataManager;
enum class NonInteractivePaymentMethodType;

// Manages logic for importing address profiles and credit card information from
// web forms into the user's Autofill profile via the `AddressDataManager` and
// the `PaymentsDataManager`. Owned by `AutofillClient` implementations.
class FormDataImporter : public AddressDataManager::Observer,
                         public history::HistoryServiceObserver {
 public:
  // Record type of the credit card extracted from the form, if one exists.
  // TODO(crbug.com/40255227): Remove this enum and user CreditCard::RecordType
  // instead.
  enum CreditCardImportType {
    // No card was successfully extracted from the form.
    kNoCard,
    // The extracted card is already stored locally on the device.
    kLocalCard,
    // The extracted card is already known to be a server card (either masked or
    // unmasked).
    kServerCard,
    // The extracted card is not currently stored with the browser.
    kNewCard,
    // The extracted card is already known to be a virtual card.
    kVirtualCard,
    // The extracted card is known to be a duplicate local and server card.
    kDuplicateLocalServerCard,
  };

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

  // Extracts credit card from the form structure.
  payments::PaymentsFormDataImporter::ExtractCreditCardFromFormResult
  ExtractCreditCardFromForm(const FormStructure& form);

  // Tries to initiate the saving of `extracted_iban` if applicable.
  bool ProcessIbanImportCandidate(Iban& extracted_iban);

  CreditCardSaveManager* GetCreditCardSaveManager() {
    return credit_card_save_manager_.get();
  }

  void AddMultiStepImportCandidate(const AutofillProfile& profile,
                                   const ProfileImportMetadata& import_metadata,
                                   bool is_imported) {
    GetAddressFormDataImporter()
        .multi_step_import_merger()
        .AddMultiStepImportCandidate(profile, import_metadata, is_imported);
  }

  // AddressDataManager::Observer
  void OnAddressDataChanged() override;

  // history::HistoryServiceObserver
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;

  // See `FormAssociator::GetFormAssociations()`.
  FormStructure::FormAssociations GetFormAssociations(
      FormSignature form_signature) const {
    return form_associator_.GetFormAssociations(form_signature);
  }

  // This should only set
  // `payment_method_type_if_non_interactive_authentication_flow_completed_` to
  // a value when there was an autofill with no interactive authentication,
  // otherwise it should set to nullopt.
  void SetPaymentMethodTypeIfNonInteractiveAuthenticationFlowCompleted(
      std::optional<NonInteractivePaymentMethodType>
          payment_method_type_if_non_interactive_authentication_flow_completed);

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

  // Returns the extracted card if one was found in the form.
  //
  // The returned card is, unless nullopt,
  // - a matching server card, if any match is found, or
  // - the candidate input card, augmented with a matching local card's nickname
  //   if such any match is found.
  // It is nullopt under the following conditions:
  // - if the card number is invalid;
  // - if the card is a known virtual card;
  // - if a card matches but the extracted card has no expiration date.
  //
  // The function has two side-effects:
  // - all matching local cards are updated to include the information from the
  //   extracted card;
  // - `credit_card_import_type_` is set to
  //   - SERVER_CARD if a server card matches;
  //   - LOCAL_CARD if a local and no server card matches;
  //   - NEW_CARD otherwise.
  std::optional<CreditCard> ExtractCreditCard(const FormStructure& form);

  // Returns an existing server card based on the following criteria:
  // - If `candidate` compares with a full server card, this function returns
  //   the existing full server card which has the same full card number as
  //   `candidate`, if one exists.
  // - If `candidate` compares with a masked server card, this function returns
  //   an existing masked server card which has the same last four digits and
  //   the same expiration date as `candidate`, if one exists.
  // additionally, set `credit_card_import_type_` set to `kServerCard`.
  // Or returns the `candidate`:
  // - If there is no matching existing server card.
  // or returns nullopt:
  // - If there is a server card which has the same number as `candidate`, but
  //   the `candidate` does not have expiration date.
  std::optional<CreditCard> TryMatchingExistingServerCard(
      const CreditCard& candidate);

  // Tries to initiate the saving of the `extracted_credit_card` if applicable.
  // `submitted_form` is the form from which the card was
  // imported. `is_credit_card_upstream_enabled` indicates if server card
  // storage is enabled. Returns true if a save is initiated.
  bool ProcessExtractedCreditCard(
      const FormStructure& submitted_form,
      const std::optional<CreditCard>& extracted_credit_card,
      bool is_credit_card_upstream_enabled,
      ukm::SourceId ukm_source_id);

  // If the mandatory re-auth opt-in bubble can be shown for a credit card, this
  // function will start the flow and return true. Otherwise, it will return
  // false.
  bool ProceedWithCardMandatoryReauthOptInIfApplicable();

  // Processes the extracted address profiles. `extracted_address_profiles`
  // contains the addresses extracted from the form. |allow_prompt| denotes if a
  // prompt can be shown. Returns true if the import of a complete profile is
  // initiated.
  bool ProcessExtractedAddressProfiles(
      const std::vector<AddressFormDataImporter::ExtractedAddressProfile>&
          extracted_address_profiles,
      bool allow_prompt,
      ukm::SourceId ukm_source_id);

  // Extracts the GUIDs of profiles used to autofill `submitted_form`, returning
  // an empty set if any field was manually edited.
  base::flat_set<std::string> ExtractGUIDsOfProfilesWithoutManualEdits(
      const FormStructure& submitted_form) const;

  PaymentsDataManager& payments_data_manager();

  // The associated autofill client.
  const raw_ref<AutofillClient> client_;

  // Responsible for managing credit card save flows (local or upload).
  std::unique_ptr<CreditCardSaveManager> credit_card_save_manager_;

  // Responsible for managing address profiles save flows.
  std::unique_ptr<AddressProfileSaveManager> address_profile_save_manager_;

  // Responsible for managing IBAN save flows. It is guaranteed to be non-null.
  std::unique_ptr<IbanSaveManager> iban_save_manager_;

  base::ScopedObservation<history::HistoryService, HistoryServiceObserver>
      history_service_observation_{this};

  base::ScopedObservation<AddressDataManager, AddressDataManager::Observer>
      address_data_manager_observation_{this};

  // Represents the type of the credit card import candidate from the submitted
  // form. It will be used to determine whether to offer upload save or not.
  // Will be passed to `credit_card_save_manager_` for metrics. If no credit
  // card was found in the form, the type will be `kNoCard`.
  CreditCardImportType credit_card_import_type_ = CreditCardImportType::kNoCard;

  // Enables associating recently submitted forms with each other.
  FormAssociator form_associator_;

  // FormDataImporter to handle address-related functionality.
  AddressFormDataImporter address_form_data_importer_;

  // FormDataImporter to handle payments-related functionality.
  payments::PaymentsFormDataImporter payments_form_data_importer_;

  // If the most recent payments autofill flow had a non-interactive
  // authentication,
  // `payment_method_type_if_non_interactive_authentication_flow_completed_`
  // will contain the type of payment method that had the non-interactive
  // authentication, otherwise it will be nullopt. This is for logging purposes
  // to log the type of non interactive payment method type that triggers
  // mandatory reauth.
  std::optional<NonInteractivePaymentMethodType>
      payment_method_type_if_non_interactive_authentication_flow_completed_;

  friend class FormDataImporterTestApi;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_FORM_DATA_IMPORTER_H_
