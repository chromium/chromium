// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_DATA_IMPORTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_DATA_IMPORTER_H_

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_profile_import_process.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/payments/credit_card_save_manager.h"
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
// Owned by BrowserAutofillManager.
class FormDataImporter : public PersonalDataManagerObserver {
 public:
  // Record type of the credit card imported from the form, if one exists.
  enum ImportedCreditCardRecordType {
    // No card was successfully imported from the form.
    NO_CARD,
    // The imported card is already stored locally on the device.
    LOCAL_CARD,
    // The imported card is already known to be a server card (either masked or
    // unmasked).
    SERVER_CARD,
    // The imported card is not currently stored with the browser.
    NEW_CARD,
  };

  // The parameters should outlive the FormDataImporter.
  FormDataImporter(AutofillClient* client,
                   payments::PaymentsClient* payments_client,
                   PersonalDataManager* personal_data_manager,
                   const std::string& app_locale);

  FormDataImporter(const FormDataImporter&) = delete;
  FormDataImporter& operator=(const FormDataImporter&) = delete;

  ~FormDataImporter() override;

  // Imports the form data, submitted by the user, into
  // |personal_data_manager_|. If a new credit card was detected and
  // |credit_card_autofill_enabled| is set to |true|, also begins the process to
  // offer local or upload credit card save.
  void ImportFormData(const FormStructure& submitted_form,
                      bool profile_autofill_enabled,
                      bool credit_card_autofill_enabled);

  // Extract credit card from the form structure. This function allows for
  // duplicated field types in the form.
  CreditCard ExtractCreditCardFromForm(const FormStructure& form);

  // Tries to infer the country |profile| is from, which can be useful to
  // verify whether the data is sensible. Returns a two-letter ISO country code
  // by considering, in decreasing order of priority:
  // - The country specified in |profile|
  // - The country determined by the variation service stored in
  //   |variation_country_code|
  // - The country code corresponding to |app_locale|
  static std::string GetPredictedCountryCode(
      const AutofillProfile& profile,
      const std::string& variation_country_code,
      const std::string& app_locale,
      LogBuffer* import_log_buffer);

  // Checks suitability of |profile| for adding to the user's set of profiles.
  static bool IsValidLearnableProfile(const AutofillProfile& profile,
                                      const std::string& predicted_country_code,
                                      const std::string& app_locale,
                                      LogBuffer* import_log_buffer);

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

  void ClearMultiStepImportCandidates() {
    multistep_candidates_.clear();
    multistep_candidates_origin_.reset();
  }

  // PersonalDataManagerObserver
  void OnBrowsingHistoryCleared(
      const history::DeletionInfo& deletion_info) override;

 protected:
  // Exposed for testing.
  void set_credit_card_save_manager(
      std::unique_ptr<CreditCardSaveManager> credit_card_save_manager) {
    credit_card_save_manager_ = std::move(credit_card_save_manager);
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // Exposed for testing.
  void set_local_card_migration_manager(
      std::unique_ptr<LocalCardMigrationManager> local_card_migration_manager) {
    local_card_migration_manager_ = std::move(local_card_migration_manager);
  }
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

 private:
  // Defines a candidate for address profile import.
  struct AddressProfileImportCandidate {
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

  // Scans the given |form| for importable Autofill data. If the form includes
  // sufficient address data for a new profile, it is immediately imported and
  // this function returns true. This function also returns true in cases where
  // FormDataImporter::ImportCreditCard() returns true, please refer to the
  // comment above that function for more details. If the form contains UPI data
  // and |credit_card_autofill_enabled| is true, the UPI ID will be stored into
  // |imported_upi_id| and this function will also return true.
  bool ImportFormData(const FormStructure& form,
                      bool profile_autofill_enabled,
                      bool credit_card_autofill_enabled,
                      bool should_return_local_card,
                      std::unique_ptr<CreditCard>* imported_credit_card,
                      std::vector<AddressProfileImportCandidate>&
                          address_profile_import_candidates,
                      absl::optional<std::string>* imported_upi_id);

  // Go through the |form| fields and attempt to extract and import valid
  // address profiles. Returns true on extraction success of at least one
  // profile. There are many reasons that extraction may fail (see
  // implementation).  The function returns true if at least one complete
  // address profile was found.
  bool ImportAddressProfiles(
      const FormStructure& form,
      std::vector<AddressProfileImportCandidate>& import_candidates);

  // Helper method for ImportAddressProfiles which only considers the fields for
  // a specified |section|. If |section| is the empty string, the import is
  // performed on the union of all sections.
  bool ImportAddressProfileForSection(
      const FormStructure& form,
      const std::string& section,
      std::vector<AddressProfileImportCandidate>& import_candidates,
      LogBuffer* import_log_buffer);

  // Go through the |form| fields and attempt to extract a new credit card in
  // |imported_credit_card|, or update an existing card. If we can find a local
  // card or server card that matches the card in the form, then it will always
  // be set in |imported_credit_card| and |imported_credit_card_record_type_|
  // will be set to the corresponding credit card record type (for example,
  // LOCAL_CARD). If we cannot find a local card or server card that matches the
  // card in the form, we will set |imported_credit_card| to the extracted card
  // from the form and |imported_credit_card_record_type_| will be set to
  // NEW_CARD. In cases where we have both a server card and local card entry
  // for |imported_credit_card|, we will update the local card entry but set
  // |imported_credit_card| to the server card data as that is the source of
  // truth, and |imported_credit_card_record_type_| will be SERVER_CARD. This
  // function returns true if the extracted card is saveable (such as if it is a
  // new card or a local card with upload enabled) or if it resulted in updating
  // the data of a local card.
  bool ImportCreditCard(const FormStructure& form,
                        bool should_return_local_card,
                        std::unique_ptr<CreditCard>* imported_credit_card);

  // Tries to initiate the saving of |imported_credit_card| if applicable.
  // |submitted_form| is the form from which the card was imported.
  // If a UPI id was found it is stored in |detected_upi_id|.
  // |credit_card_autofill_enabled| indicates if credit card filling is enabled
  // and |is_credit_card_upstream_enabled| indicates if server card storage is
  // enabled. Returns true if a save is initiated.
  bool ProcessCreditCardImportCandidate(
      const FormStructure& submitted_form,
      std::unique_ptr<CreditCard> imported_credit_card,
      absl::optional<std::string> detected_upi_id,
      bool credit_card_autofill_enabled,
      bool is_credit_card_upstream_enabled);

  // Processes the address profile import candidates.
  // |import_candidates| contains the addresses extracted from the form.
  // |allow_prompt| denotes if a prompt can be shown.
  // Returns true if the import of a complete profile is initiated.
  bool ProcessAddressProfileImportCandidates(
      const std::vector<AddressProfileImportCandidate>& import_candidates,
      bool allow_prompt = true);

  // Extracts credit card from the form structure. |hasDuplicateFieldType| will
  // be set as true if there are duplicated field types in the form.
  CreditCard ExtractCreditCardFromForm(const FormStructure& form,
                                       bool* hasDuplicateFieldType);

  // Go through the |form| fields and find a UPI ID to import. The return value
  // will be empty if no UPI ID was found.
  absl::optional<std::string> ImportUpiId(const FormStructure& form);

  // |imported_credit_card| stores a pointer to the card imported from the form.
  // If no valid card was imported, it is set to nullptr. It might be set to a
  // copy of a LOCAL_CARD or SERVER_CARD we have already saved if we were able
  // to find a matching copy. |is_credit_card_upstream_enabled| denotes whether
  // the user has credit card upload enabled. This function is used to prevent
  // offering upload card save or local card save in situations where it would
  // be invalid to offer them. For example, we should not offer to upload card
  // if it is already a server card.
  bool ShouldOfferUploadCardOrLocalCardSave(
      const CreditCard* imported_credit_card,
      bool is_credit_card_upload_enabled);

  // If `kAutofillComplementCountryCodeOnImport` is enabled and the `profile`'s
  // country is not empty, complements it with `predicted_country_code`. To give
  // users the opportunity to edit, this is only done with explicit save prompts
  // enabled.
  // Returns true if the country was complemented.
  bool ComplementCountry(AutofillProfile& profile,
                         const std::string& predicted_country_code);

  // Sets the `profile`'s PHONE_HOME_WHOLE_NUMBER to the `combined_phone`, if
  // possible. Deduces the region based on `predicted_country_code`.
  // Returns false if the provided `combined_phone` is invalid.
  // TODO(crbug.com/1297032): Remove `predicted_country_code` when launched.
  bool SetPhoneNumber(AutofillProfile& profile,
                      PhoneNumber::PhoneCombineHelper& combined_phone,
                      const std::string& predicted_country_code);

  // Clears all setting-inaccessible values from `profile` if
  // `kAutofillRemoveInaccessibleProfileValues` is enabled.
  // TODO(crbug.com/1297032): Remove `predicted_country_code` when launched.
  void RemoveInaccessibleProfileValues(
      AutofillProfile& profile,
      const std::string& predicted_country_code);

  // Removes updated multi-step candidates, merges |profile| with multi-step
  // candidates and potentially stores it as a multi-step candidate itself.
  // |profile| and |import_metadata| are updated accordingly, if the profile can
  // be merged. See |MergeProfileWithMultiStepCandidates()| for details.
  // Only applicable when |kAutofillEnableMultiStepImports| is enabled.
  void ProcessMultiStepImport(AutofillProfile& profile,
                              ProfileImportMetadata& import_metadata,
                              const url::Origin& origin);

  // Removes any MultiStepFormProfileCandidate from |multistep_candidates_| that
  // reached their TTL or have a different |origin|.
  void RemoveOutdatedMultiStepCandidates(const url::Origin& origin);

  // Merges a given |profile| stepwise with |multistep_candidates_| to
  // complete it. |profile| is assumed to contain no invalid information.
  // Returns true if the resulting profile satisfies the minimum address
  // requirements. |profile| and |import_metadata| are updated in this case with
  // the result of merging all relevant candidates.
  // Returns false otherwise and leaves |profile| and |import_metadata|
  // unchanged.
  // Any merged or colliding |multistep_candidates_| are cleared.
  // |origin|: The origin of the form where |profile| was imported from.
  bool MergeProfileWithMultiStepCandidates(
      AutofillProfile& profile,
      ProfileImportMetadata& import_metadata,
      const url::Origin& origin);

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

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // Responsible for migrating locally saved credit cards to Google Pay.
  std::unique_ptr<LocalCardMigrationManager> local_card_migration_manager_;

  // Responsible for managing UPI/VPA save flows.
  std::unique_ptr<UpiVpaSaveManager> upi_vpa_save_manager_;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  // The personal data manager, used to save and load personal data to/from the
  // web database.  This is overridden by the BrowserAutofillManagerTest.
  // Weak reference.
  // May be NULL.  NULL indicates OTR.
  raw_ptr<PersonalDataManager> personal_data_manager_;

  // Represents the type of the imported credit card from the submitted form.
  // It will be used to determine whether to offer Upstream or card migration.
  // Will be passed to |credit_card_save_manager_| for metrics.
  ImportedCreditCardRecordType imported_credit_card_record_type_;

  std::string app_locale_;

  // Used to store the last four digits of the fetched virtual cards.
  base::flat_set<std::u16string> fetched_virtual_cards_;

  // Responsible for managing the virtual card enrollment flow through chrome.
  std::unique_ptr<VirtualCardEnrollmentManager>
      virtual_card_enrollment_manager_;

  // Represents a submitted form, stored to be considered as a merge candidate
  // for other candidate profiles in future submits in a multi-step import flow.
  struct MultiStepFormProfileCandidate {
    // The import candidate.
    AutofillProfile profile;
    // Metadata about how |profile| was constructed.
    ProfileImportMetadata import_metadata;
    // Timestamp when the submit happened.
    base::Time timestamp;
  };
  // Current multi-step import candidates, in increasing order of their
  // |timestamp|.
  std::deque<MultiStepFormProfileCandidate> multistep_candidates_;
  // All |multistep_candidates_| share the same origin. Has a value iff
  // |multistep_candidates_| is not empty.
  absl::optional<url::Origin> multistep_candidates_origin_;

  friend class AutofillMergeTest;
  friend class FormDataImporterTest;
  friend class FormDataImporterTestBase;
  friend class LocalCardMigrationBrowserTest;
  friend class SaveCardBubbleViewsFullFormBrowserTest;
  friend class SaveCardInfobarEGTestHelper;
  friend class ::SaveCardOfferObserver;
  FRIEND_TEST_ALL_PREFIXES(AutofillMergeTest, MergeProfiles);
  FRIEND_TEST_ALL_PREFIXES(FormDataImporterNonParameterizedTest,
                           ProcessCreditCardImportCandidate_EmptyCreditCard);
  FRIEND_TEST_ALL_PREFIXES(FormDataImporterNonParameterizedTest,
                           ShouldOfferUploadCardOrLocalCardSave);
  FRIEND_TEST_ALL_PREFIXES(FormDataImporterTest,
                           AllowDuplicateMaskedServerCardIfFlagEnabled);
  FRIEND_TEST_ALL_PREFIXES(
      FormDataImporterTest,
      DuplicateFullServerCardWhileContainingLocalCardCopies);
  FRIEND_TEST_ALL_PREFIXES(FormDataImporterTest, DuplicateMaskedServerCard);
  FRIEND_TEST_ALL_PREFIXES(
      FormDataImporterTest,
      ImportCreditCard_DuplicateServerCards_ExtractFullCard);
  FRIEND_TEST_ALL_PREFIXES(
      FormDataImporterTest,
      ImportCreditCard_DuplicateServerCards_ExtractMaskedCard);
  FRIEND_TEST_ALL_PREFIXES(FormDataImporterTest,
                           ImportFormData_AddressesDisabledOneCreditCard);
  FRIEND_TEST_ALL_PREFIXES(FormDataImporterTest,
                           ImportFormData_AddressCreditCardDisabled);
  FRIEND_TEST_ALL_PREFIXES(FormDataImporterTest,
                           ImportFormData_HiddenCreditCardFormAfterEntered);
  FRIEND_TEST_ALL_PREFIXES(
      FormDataImporterTest,
      ImportFormData_ImportCreditCardRecordType_FullServerCard);
  FRIEND_TEST_ALL_PREFIXES(FormDataImporterTest,
                           ImportFormData_ImportCreditCardRecordType_LocalCard);
  FRIEND_TEST_ALL_PREFIXES(
      FormDataImporterTest,
      ImportFormData_ImportCreditCardRecordType_MaskedServerCard);
  FRIEND_TEST_ALL_PREFIXES(FormDataImporterTest,
                           ImportFormData_ImportCreditCardRecordType_NewCard);
  FRIEND_TEST_ALL_PREFIXES(
      FormDataImporterTest,
      ImportFormData_ImportCreditCardRecordType_NoCard_ExpiredCard_EditableExpDateOff);
  FRIEND_TEST_ALL_PREFIXES(
      FormDataImporterTest,
      ImportFormData_ImportCreditCardRecordType_NewCard_ExpiredCard_WithExpDateFixFlow);
  FRIEND_TEST_ALL_PREFIXES(
      FormDataImporterTest,
      ImportFormData_ImportCreditCardRecordType_NoCard_InvalidCardNumber);
  FRIEND_TEST_ALL_PREFIXES(
      FormDataImporterTest,
      ImportFormData_ImportCreditCardRecordType_NoCard_VirtualCard);
  FRIEND_TEST_ALL_PREFIXES(
      FormDataImporterTest,
      ImportFormData_ImportCreditCardRecordType_NoCard_NoCardOnForm);
  FRIEND_TEST_ALL_PREFIXES(FormDataImporterTest,
                           ImportFormData_OneAddressCreditCardDisabled);
  FRIEND_TEST_ALL_PREFIXES(FormDataImporterTest,
                           ImportFormData_OneAddressOneCreditCard);
  FRIEND_TEST_ALL_PREFIXES(
      FormDataImporterTest,
      ImportFormData_SecondImportResetsCreditCardRecordType);
  FRIEND_TEST_ALL_PREFIXES(FormDataImporterTest,
                           ImportFormData_TwoAddressesOneCreditCard);
  FRIEND_TEST_ALL_PREFIXES(FormDataImporterTest,
                           ImportFormData_DontSetUpiIdWhenOnlyCreditCardExists);
  FRIEND_TEST_ALL_PREFIXES(
      FormDataImporterTest,
      Metrics_SubmittedServerCardExpirationStatus_FullServerCardMatch);
  FRIEND_TEST_ALL_PREFIXES(
      FormDataImporterTest,
      Metrics_SubmittedServerCardExpirationStatus_FullServerCardMismatch);
  FRIEND_TEST_ALL_PREFIXES(
      FormDataImporterTest,
      Metrics_SubmittedServerCardExpirationStatus_MaskedServerCardMatch);
  FRIEND_TEST_ALL_PREFIXES(
      FormDataImporterTest,
      Metrics_SubmittedServerCardExpirationStatus_MaskedServerCardMismatch);
  FRIEND_TEST_ALL_PREFIXES(
      FormDataImporterTest,
      Metrics_SubmittedServerCardExpirationStatus_EmptyExpirationMonth);
  FRIEND_TEST_ALL_PREFIXES(
      FormDataImporterTest,
      Metrics_SubmittedServerCardExpirationStatus_EmptyExpirationYear);
  FRIEND_TEST_ALL_PREFIXES(
      FormDataImporterTest,
      Metrics_SubmittedDifferentServerCardExpirationStatus_EmptyExpirationYear);
  FRIEND_TEST_ALL_PREFIXES(FormDataImporterTest, ImportUpiId);
  FRIEND_TEST_ALL_PREFIXES(FormDataImporterTest, ImportUpiIdDisabled);
  FRIEND_TEST_ALL_PREFIXES(FormDataImporterTest, ImportUpiIdIgnoreNonUpiId);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_DATA_IMPORTER_H_
