// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_DATA_IMPORTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_DATA_IMPORTER_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_profile_import_process.h"
#include "components/autofill/core/browser/form_data_importer_utils.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/payments/credit_card_save_manager.h"
#include "components/autofill/core/browser/payments/iban_save_manager.h"
#include "components/autofill/core/browser/payments/local_card_migration_manager.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/payments/upi_vpa_save_manager.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class SaveCardOfferObserver;

namespace autofill {

class AddressProfileSaveManager;

// Manages logic for importing address profiles and credit card information from
// web forms into the user's Autofill profile via the PersonalDataManager.
// Owned by `ChromeAutofillClient`.
class FormDataImporter : public PersonalDataManagerObserver {
 public:
  // Record type of the credit card extracted from the form, if one exists.
  // TODO(crbug.com/1412326): Remove this enum and user CreditCard::RecordType
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
  };

  // The parameters should outlive the FormDataImporter.
  FormDataImporter(AutofillClient* client,
                   payments::PaymentsClient* payments_client,
                   PersonalDataManager* personal_data_manager,
                   const std::string& app_locale);

  FormDataImporter(const FormDataImporter&) = delete;
  FormDataImporter& operator=(const FormDataImporter&) = delete;

  ~FormDataImporter() override;

  using CardGuid = base::StrongAlias<class CardGuidTag, std::string>;
  using CardLastFourDigits =
      base::StrongAlias<class CardLastFourDigitsTag, std::string>;

  // Imports the form data, submitted by the user, into
  // `personal_data_manager_`. If a new credit card was detected and
  // `payment_methods_autofill_enabled` is set to `true`, also begins the
  // process to offer local or upload credit card save.
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

  // Tries to initiate the saving of `iban_import_candidate` if applicable.
  bool ProcessIBANImportCandidate(const IBAN& iban_import_candidate);

  // Cache the last four of the fetched virtual card so we don't offer saving
  // them.
  void CacheFetchedVirtualCard(const std::u16string& last_four);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  LocalCardMigrationManager* local_card_migration_manager() {
    return local_card_migration_manager_.get();
  }
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  VirtualCardEnrollmentManager* GetVirtualCardEnrollmentManager() {
    return virtual_card_enrollment_manager_.get();
  }

  void AddMultiStepImportCandidate(const AutofillProfile& profile,
                                   const ProfileImportMetadata& import_metadata,
                                   bool is_imported) {
    multistep_importer_.AddMultiStepImportCandidate(profile, import_metadata,
                                                    is_imported);
  }

  // See comment for |fetched_card_instrument_id_|.
  void SetFetchedCardInstrumentId(int64_t instrument_id);

  // PersonalDataManagerObserver
  void OnPersonalDataChanged() override;
  void OnBrowsingHistoryCleared(
      const history::DeletionInfo& deletion_info) override;

  // See `FormAssociator::GetFormAssociations()`.
  absl::optional<FormStructure::FormAssociations> GetFormAssociations(
      FormSignature form_signature) const {
    return form_associator_.GetFormAssociations(form_signature);
  }

  CreditCardImportType credit_card_import_type_for_testing() const {
    return credit_card_import_type_;
  }
  void set_credit_card_import_type_for_testing(
      CreditCardImportType credit_card_import_type) {
    credit_card_import_type_ = credit_card_import_type;
  }

  IBANSaveManager* iban_save_manager_for_testing() {
    return iban_save_manager_.get();
  }

  // This should only set
  // `card_identifier_if_non_interactive_authentication_flow_completed_` to a
  // value when there was an autofill with no interactive authentication,
  // otherwise it should set to nullopt. If we are in the virtual card case,
  // this will be set to the last four digits of the virtual card number.
  // Otherwise, this will be set to the GUID of the card.
  void SetCardIdentifierIfNonInteractiveAuthenticationFlowCompleted(
      absl::optional<absl::variant<CardGuid, CardLastFourDigits>>
          card_identifier_if_non_interactive_authentication_flow_completed);
  const absl::optional<absl::variant<CardGuid, CardLastFourDigits>>&
  GetCardIdentifierIfNonInteractiveAuthenticationFlowCompleted() const;

  bool ProcessExtractedCreditCardForTesting(
      const FormStructure& submitted_form,
      const absl::optional<CreditCard>& credit_card_import_candidate,
      const absl::optional<std::string>& extracted_upi_id,
      bool payment_methods_autofill_enabled,
      bool is_credit_card_upstream_enabled) {
    return ProcessExtractedCreditCard(
        submitted_form, credit_card_import_candidate, extracted_upi_id,
        payment_methods_autofill_enabled, is_credit_card_upstream_enabled);
  }

 protected:
  void set_credit_card_save_manager_for_testing(
      std::unique_ptr<CreditCardSaveManager> credit_card_save_manager) {
    credit_card_save_manager_ = std::move(credit_card_save_manager);
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  void set_iban_save_manager_for_testing(
      std::unique_ptr<IBANSaveManager> iban_save_manager) {
    iban_save_manager_ = std::move(iban_save_manager);
  }
  void set_local_card_migration_manager_for_testing(
      std::unique_ptr<LocalCardMigrationManager> local_card_migration_manager) {
    local_card_migration_manager_ = std::move(local_card_migration_manager);
  }
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  // The instrument id of the card that has been most recently retrieved via
  // Autofill Downstream (card retrieval from server). This can be used to
  // decide whether the card submitted is the same card retrieved. This field is
  // optional and is set when an Autofill Downstream has happened.
  absl::optional<int64_t> fetched_card_instrument_id_;

 private:
  // Defines a candidate for address profile import.
  struct AddressProfileImportCandidate {
    AddressProfileImportCandidate();
    AddressProfileImportCandidate(const AddressProfileImportCandidate& other);
    ~AddressProfileImportCandidate();

    // The profile that was extracted from the form.
    AutofillProfile profile;
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
    absl::optional<CreditCard> extracted_credit_card;
    // List of address profiles extracted from the form, which are candidates
    // for importing. The list is empty if none of the address profile fulfill
    // import requirements.
    std::vector<AddressProfileImportCandidate>
        address_profile_import_candidates;
    // IBAN extracted from the form, which is a candidate for importing. Present
    // if an IBAN is found in the form.
    absl::optional<IBAN> iban_import_candidate;
    // Present if a UPI (Unified Payment Interface) ID is found in the form.
    absl::optional<std::string> extracted_upi_id;
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

  // Validates that the required fields in the `profile` have values, based on
  // the requirements of the `profile`'s country. Accordingly, logs the form
  // import requirement metrics.
  bool LogAddressFormImportRequirementMetric(const AutofillProfile& profile,
                                             LogBuffer* import_log_buffer);

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
  absl::optional<CreditCard> ExtractCreditCard(const FormStructure& form);

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
  absl::optional<CreditCard> TryMatchingExistingServerCard(
      const CreditCard& candidate);

  // Returns the extracted IBAN from the `form` if it is a new IBAN.
  absl::optional<IBAN> ExtractIBAN(const FormStructure& form);

  // Tries to initiate the saving of the `extracted_credit_card` if applicable.
  // `submitted_form` is the form from which the card was
  // imported. If a UPI id was found it is stored in `extracted_upi_id`.
  // `is_credit_card_upstream_enabled` indicates if server card storage is
  // enabled. Returns true if a save is initiated.
  bool ProcessExtractedCreditCard(
      const FormStructure& submitted_form,
      const absl::optional<CreditCard>& extracted_credit_card,
      const absl::optional<std::string>& extracted_upi_id,
      bool payment_methods_autofill_enabled,
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
  IBAN ExtractIBANFromForm(const FormStructure& form);

  // Go through the `form` fields and find a UPI ID to extract. The return value
  // will be empty if no UPI ID was found.
  absl::optional<std::string> ExtractUpiId(const FormStructure& form);

  // Returns true if credit card upload or local save should be offered to user.
  // `extracted_credit_card` is the credit card imported from the form if there
  // is any. If no valid card was imported, it is set to nullopt. It might be
  // set to a copy of a LOCAL_CARD or SERVER_CARD we have already saved if we
  // were able to find a matching copy. |is_credit_card_upstream_enabled|
  // denotes whether the user has credit card upload enabled. This function is
  // used to prevent offering upload card save or local card save in situations
  // where it would be invalid to offer them. For example, we should not offer
  // to upload card if it is already a valid server card.
  bool ShouldOfferUploadCardOrLocalCardSave(
      const absl::optional<CreditCard>& extracted_credit_card,
      bool is_credit_card_upload_enabled);

  // If the `profile`'s country is not empty, complements it with
  // `predicted_country_code`. To give users the opportunity to edit, this is
  // only done with explicit save prompts enabled.
  // Returns true if the country was complemented.
  bool ComplementCountry(AutofillProfile& profile,
                         const std::string& predicted_country_code);

  // Sets the `profile`'s PHONE_HOME_WHOLE_NUMBER to the `combined_phone`, if
  // possible. The phone number's region is deduced based on the profile's
  // country or alternatively the app locale.
  // Returns false if the provided `combined_phone` is invalid.
  bool SetPhoneNumber(AutofillProfile& profile,
                      PhoneNumber::PhoneCombineHelper& combined_phone);

  // Clears all setting-inaccessible values from `profile` if
  // `kAutofillRemoveInaccessibleProfileValues` is enabled.
  void RemoveInaccessibleProfileValues(AutofillProfile& profile);

  // Whether a dynamic change form is imported.
  bool from_dynamic_change_form_ = false;

  // Whether the form imported has non-focusable fields after user entered
  // information into it.
  bool has_non_focusable_field_ = false;

  // The associated autofill client. Weak reference.
  raw_ptr<AutofillClient> client_;

  // Responsible for managing credit card save flows (local or upload).
  std::unique_ptr<CreditCardSaveManager> credit_card_save_manager_;

  // Responsible for managing address profiles save flows.
  std::unique_ptr<AddressProfileSaveManager> address_profile_save_manager_;

  // Responsible for managing IBAN save flows.
  std::unique_ptr<IBANSaveManager> iban_save_manager_;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // Responsible for migrating locally saved credit cards to Google Pay.
  std::unique_ptr<LocalCardMigrationManager> local_card_migration_manager_;

  // Responsible for managing UPI/VPA save flows.
  std::unique_ptr<UpiVpaSaveManager> upi_vpa_save_manager_;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  // The personal data manager, used to save and load personal data to/from the
  // web database.  This is overridden by the BrowserAutofillManagerTest.
  // Weak reference.
  raw_ptr<PersonalDataManager> personal_data_manager_;

  // Represents the type of the credit card import candidate from the submitted
  // form. It will be used to determine whether to offer upload save or card
  // migration. Will be passed to `credit_card_save_manager_` for metrics. If no
  // credit card was found in the form, the type will be `kNoCard`.
  CreditCardImportType credit_card_import_type_ = CreditCardImportType::kNoCard;

  std::string app_locale_;

  // Used to store the last four digits of the fetched virtual cards.
  base::flat_set<std::u16string> fetched_virtual_cards_;

  // Responsible for managing the virtual card enrollment flow through chrome.
  std::unique_ptr<VirtualCardEnrollmentManager>
      virtual_card_enrollment_manager_;

  // Enables importing from multi-step import flows.
  MultiStepImportMerger multistep_importer_;

  // Enables associating recently submitted forms with each other.
  FormAssociator form_associator_;

  // Optional that will have a value when the most recent payments autofill flow
  // had no interactive authentication. It will contain the GUID or last four
  // digits of the card where the most recent non-interactive authentication has
  // succeeded. If this is empty upon form submission, it implies that the most
  // recent autofill had an interactive authentication. Set when
  // `SetCardIdentifierIfNonInteractiveAuthenticationFlowCompleted()` is called,
  // and cleared on page navigation.
  absl::optional<absl::variant<CardGuid, CardLastFourDigits>>
      card_identifier_if_non_interactive_authentication_flow_completed_;

  friend class AutofillMergeTest;
  friend class FormDataImporterTest;
  friend class FormDataImporterTestBase;
  friend class LocalCardMigrationBrowserTest;
  friend class SaveCardBubbleViewsFullFormBrowserTest;
  friend class SaveCardInfobarEGTestHelper;
  friend class ::SaveCardOfferObserver;
  FRIEND_TEST_ALL_PREFIXES(FormDataImporterNonParameterizedTest,
                           ShouldOfferUploadCardOrLocalCardSave);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_DATA_IMPORTER_H_
