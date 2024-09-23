// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_DATA_IMPORTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_DATA_IMPORTER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_profile_import_process.h"
#include "components/autofill/core/browser/form_data_importer_utils.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/history/core/browser/history_service_observer.h"

namespace history {
class HistoryService;
}  // namespace history

namespace autofill {

class AddressProfileSaveManager;
class AutofillClient;
class CreditCardSaveManager;
class IbanSaveManager;
class LocalCardMigrationManager;
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
                   history::HistoryService* history_service,
                   const std::string& app_locale);

  FormDataImporter(const FormDataImporter&) = delete;
  FormDataImporter& operator=(const FormDataImporter&) = delete;

  ~FormDataImporter() override;

  // Imports the form data submitted by the user. If a new credit card was
  // detected and `payment_methods_autofill_enabled` is set to `true`, also
  // begins the process to offer local or upload credit card save.
  void ImportAndProcessFormData(const FormStructure& submitted_form,
                                bool profile_autofill_enabled,
                                bool payment_methods_autofill_enabled);

  struct ExtractCreditCardFromFormResult {
    // The extracted credit card, which may be a candidate for import.
    // If there is no credit card field in the form, the value is the default
    // `CreditCard()`.
    CreditCard card;
    // If there are multiple credit card fields of the same type in the form, we
    // won't know which value to import.
    bool has_duplicate_credit_card_field_type = false;
  };

  // Extracts credit card from the form structure.
  ExtractCreditCardFromFormResult ExtractCreditCardFromForm(
      const FormStructure& form);

  // Tries to initiate the saving of `extracted_iban` if applicable.
  bool ProcessIbanImportCandidate(Iban& extracted_iban);

  // Cache the last four of the fetched virtual card so we don't offer saving
  // them.
  void CacheFetchedVirtualCard(const std::u16string& last_four);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  LocalCardMigrationManager* local_card_migration_manager() {
    return local_card_migration_manager_.get();
  }
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  CreditCardSaveManager* GetCreditCardSaveManager() {
    return credit_card_save_manager_.get();
  }

  void AddMultiStepImportCandidate(const AutofillProfile& profile,
                                   const ProfileImportMetadata& import_metadata,
                                   bool is_imported) {
    multistep_importer_.AddMultiStepImportCandidate(profile, import_metadata,
                                                    is_imported);
  }

  // See comment for |fetched_card_instrument_id_|.
  void SetFetchedCardInstrumentId(int64_t instrument_id);

  // AddressDataManager::Observer
  void OnAddressDataChanged() override;

  // history::HistoryServiceObserver
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;

  // See `FormAssociator::GetFormAssociations()`.
  std::optional<FormStructure::FormAssociations> GetFormAssociations(
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

 private:
  // Defines a candidate for address profile import.
  struct AddressProfileImportCandidate {
    AddressProfileImportCandidate();
    AddressProfileImportCandidate(const AddressProfileImportCandidate& other);
    ~AddressProfileImportCandidate();

    // The profile that was extracted from the form.
    AutofillProfile profile{i18n_model_definition::kLegacyHierarchyCountryCode};
    // The URL the profile was extracted from.
    GURL url;
    // Indicates if all import requirements have been fulfilled.
    bool all_requirements_fulfilled;
    // Metadata about the import, used for metric collection in
    // ProfileImportProcess after the user's decision.
    ProfileImportMetadata import_metadata;
  };

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
    std::vector<AddressProfileImportCandidate>
        address_profile_import_candidates;
    // IBAN extracted from the form, which is a candidate for importing. Present
    // if an IBAN is found in the form.
    std::optional<Iban> extracted_iban;
  };

  // Scans the given `form` for extractable Autofill data.
  ExtractedFormData ExtractFormData(const FormStructure& form,
                                    bool profile_autofill_enabled,
                                    bool payment_methods_autofill_enabled);

  // Attempts to construct AddressProfileImportCandidates by extracting values
  // from the fields in the `form`'s sections. Extraction can fail if the
  // fields' values don't pass validation. Apart from complete address profiles,
  // partial profiles for silent updates are extracted. All are stored in
  // `extracted_form_data`'s `address_profile_import_candidates`.
  // The function returns the number of _complete_ extracted profiles.
  size_t ExtractAddressProfiles(const FormStructure& form,
                                std::vector<AddressProfileImportCandidate>*
                                    address_profile_import_candidates);

  // Iterates over `section_fields` and builds a map from field type to observed
  // value for that field type.
  base::flat_map<FieldType, std::u16string> GetAddressObservedFieldValues(
      base::span<const AutofillField* const> section_fields,
      ProfileImportMetadata& import_metadata,
      LogBuffer* import_log_buffer,
      bool& has_invalid_field_types,
      bool& has_multiple_distinct_email_addresses,
      bool& has_address_related_fields) const;

  // Helper method to construct an AutofillProfile out of observed values in the
  // form. Used during `ExtractAddressProfileFromSection()`.
  AutofillProfile ConstructProfileFromObservedValues(
      const base::flat_map<FieldType, std::u16string>& observed_values,
      LogBuffer* import_log_buffer,
      ProfileImportMetadata& import_metadata);

  // Helper method for ImportAddressProfiles which only considers the fields
  // for a specified `section`. If no section is passed, the import is
  // performed on the union of all sections.
  bool ExtractAddressProfileFromSection(
      base::span<const AutofillField* const> section_fields,
      const GURL& source_url,
      std::vector<AddressProfileImportCandidate>*
          address_profile_import_candidates,
      LogBuffer* import_log_buffer);

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

  // Returns the extracted IBAN from the `form` if it is a new IBAN.
  std::optional<Iban> ExtractIban(const FormStructure& form);

  // Tries to initiate the saving of the `extracted_credit_card` if applicable.
  // `submitted_form` is the form from which the card was
  // imported. `is_credit_card_upstream_enabled` indicates if server card
  // storage is enabled. Returns true if a save is initiated.
  bool ProcessExtractedCreditCard(
      const FormStructure& submitted_form,
      const std::optional<CreditCard>& extracted_credit_card,
      bool is_credit_card_upstream_enabled);

  // Processes the address profile import candidates.
  // |address_profile_import_candidates| contains the addresses extracted
  // from the form. |allow_prompt| denotes if a prompt can be shown.
  // Returns true if the import of a complete profile is initiated.
  bool ProcessAddressProfileImportCandidates(
      const std::vector<AddressProfileImportCandidate>&
          address_profile_import_candidates,
      bool allow_prompt = true);

  // Helper function which extracts the IBAN from the form structure.
  Iban ExtractIbanFromForm(const FormStructure& form);

  // Returns true if credit card upload, local save, or cvc local save should be
  // offered to user. `extracted_credit_card` is the credit card imported from
  // the form if there is any. If no valid card was imported, it is set to
  // nullopt. It might be set to a copy of a LOCAL_CARD or SERVER_CARD we have
  // already saved if we were able to find a matching copy.
  // |is_credit_card_upstream_enabled| denotes whether the user has credit card
  // upload enabled. This function is used to prevent offering upload card save
  // or local card save in situations where it would be invalid to offer them.
  // For example, we should not offer to upload card if it is already a valid
  // server card.
  // TODO(crbug.com/40270301): Move to CreditCardSaveManger.
  bool ShouldOfferCreditCardSave(
      const std::optional<CreditCard>& extracted_credit_card,
      bool is_credit_card_upstream_enabled);

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

  // Clears all setting-inaccessible values from `profile` if
  // `kAutofillRemoveInaccessibleProfileValues` is enabled.
  void RemoveInaccessibleProfileValues(AutofillProfile& profile);

  AddressDataManager& address_data_manager();

  PaymentsDataManager& payments_data_manager();

  // The associated autofill client.
  const raw_ref<AutofillClient> client_;

  // Responsible for managing credit card save flows (local or upload).
  std::unique_ptr<CreditCardSaveManager> credit_card_save_manager_;

  // Responsible for managing address profiles save flows.
  std::unique_ptr<AddressProfileSaveManager> address_profile_save_manager_;

  // Responsible for managing IBAN save flows. It is guaranteed to be non-null.
  std::unique_ptr<IbanSaveManager> iban_save_manager_;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // Responsible for migrating locally saved credit cards to Google Pay.
  std::unique_ptr<LocalCardMigrationManager> local_card_migration_manager_;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  base::ScopedObservation<history::HistoryService, HistoryServiceObserver>
      history_service_observation_{this};

  base::ScopedObservation<AddressDataManager, AddressDataManager::Observer>
      address_data_manager_observation_{this};

  // Represents the type of the credit card import candidate from the submitted
  // form. It will be used to determine whether to offer upload save or card
  // migration. Will be passed to `credit_card_save_manager_` for metrics. If no
  // credit card was found in the form, the type will be `kNoCard`.
  CreditCardImportType credit_card_import_type_ = CreditCardImportType::kNoCard;

  std::string app_locale_;

  // Used to store the last four digits of the fetched virtual cards.
  base::flat_set<std::u16string> fetched_virtual_cards_;

  // Enables importing from multi-step import flows.
  MultiStepImportMerger multistep_importer_;

  // Enables associating recently submitted forms with each other.
  FormAssociator form_associator_;

  // If the most recent payments autofill flow had a non-interactive
  // authentication,
  // `payment_method_type_if_non_interactive_authentication_flow_completed_`
  // will contain the type of payment method that had the non-interactive
  // authentication, otherwise it will be nullopt. This is for logging purposes
  // to log the type of non interactive payment method type that triggers
  // mandatory reauth.
  std::optional<NonInteractivePaymentMethodType>
      payment_method_type_if_non_interactive_authentication_flow_completed_;

  // The instrument id of the card that has been most recently retrieved via
  // Autofill Downstream (card retrieval from server). This can be used to
  // decide whether the card submitted is the same card retrieved. This field is
  // optional and is set when an Autofill Downstream has happened.
  std::optional<int64_t> fetched_card_instrument_id_;

  friend class FormDataImporterTestApi;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_DATA_IMPORTER_H_
