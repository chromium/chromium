// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_SAVE_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_SAVE_MANAGER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/strike_databases/payments/credit_card_save_strike_database.h"
#include "components/autofill/core/browser/strike_databases/payments/cvc_storage_strike_database.h"
#include "components/autofill/core/browser/strike_databases/payments/local_card_migration_strike_database.h"
#include "url/origin.h"

class SaveCardOfferObserver;

namespace autofill {

class AutofillClient;

// Time in sec to wait before showing virtual card enrollment if save card
// confirmation prompt is still visible.
inline constexpr base::TimeDelta kVirtualCardEnrollDelaySec = base::Seconds(3);

// Manages logic for determining whether upload credit card save to Google
// Payments is available as well as actioning both local and upload credit card
// save logic.  Owned by FormDataImporter.
class CreditCardSaveManager {
 public:
  // Possible fields and values detected during credit card form submission, to
  // be sent to Google Payments to better determine if upload credit card save
  // should be offered.  These should stay consistent with the equivalent enum
  // in Google Payments code.
  enum DetectedValue {
    // Set if a valid CVC was detected.  Will always be set if the CVC fix flow
    // is enabled.
    CVC = 1 << 0,
    // Set if a cardholder name was found, *unless* conflicting names were
    // found.
    CARDHOLDER_NAME = 1 << 1,
    // Set if an address name was found, *unless* conflicting names were found.
    ADDRESS_NAME = 1 << 2,
    // Set if an address line was found in any address (regardless of
    // conflicts).
    ADDRESS_LINE = 1 << 3,
    // Set if a locality was found in any address (regardless of conflicts).
    LOCALITY = 1 << 4,
    // Set if an administrative area was found in any address (regardless of
    // conflicts).
    ADMINISTRATIVE_AREA = 1 << 5,
    // Set if a postal code was found in any address, *unless* conflicting
    // postal codes were found.
    POSTAL_CODE = 1 << 6,
    // Set if a country code was found in any address (regardless of conflicts).
    COUNTRY_CODE = 1 << 7,
    // Set if the user is already syncing data from a Google Payments account.
    HAS_GOOGLE_PAYMENTS_ACCOUNT = 1 << 8,
    // Card expiration month.
    CARD_EXPIRATION_MONTH = 1 << 9,
    // Card expiration year.
    CARD_EXPIRATION_YEAR = 1 << 10,
    // Phone number was found on any address (not currently used).
    PHONE_NUMBER = 1 << 11,
    // Set if cardholder name was explicitly requested in the offer-to-save
    // dialog.  In general, this should happen when name is conflicting/missing
    // and the user does not have a Google Payments account.
    USER_PROVIDED_NAME = 1 << 12,
    // Set if expiration date was explicitly requested in the offer-to-save
    // dialog. In general, this should happen when expiration date month or year
    // is missing.
    USER_PROVIDED_EXPIRATION_DATE = 1 << 13,
  };

  // An observer class used by browsertests that gets notified whenever
  // particular actions occur.
  class ObserverForTest {
   public:
    virtual ~ObserverForTest() = default;
    virtual void OnOfferLocalSave() {}
    virtual void OnOfferUploadSave() {}
    virtual void OnDecideToRequestUploadSave() {}
    virtual void OnReceivedGetUploadDetailsResponse() {}
    virtual void OnSentUploadCardRequest() {}
    virtual void OnReceivedUploadCardResponse() {}
    virtual void OnShowCardSavedFeedback() {}
    virtual void OnStrikeChangeComplete() {}
  };

  // `client` must outlive the CreditCardSaveManager.
  CreditCardSaveManager(AutofillClient* client, const std::string& app_locale);

  CreditCardSaveManager(const CreditCardSaveManager&) = delete;
  CreditCardSaveManager& operator=(const CreditCardSaveManager&) = delete;

  virtual ~CreditCardSaveManager();

  // Begins the process to offer local credit card save to the user. Returns
  // true if the prompt is shown.
  virtual bool AttemptToOfferCardLocalSave(const CreditCard& card);

  // Begins the process to offer local CVC save to the user. Returns true if the
  // prompt is shown. `card` is the credit card extracted from the form.
  virtual bool AttemptToOfferCvcLocalSave(const CreditCard& card);

  // Returns true if CVC local or upload save should be offered to the user.
  // `card` is the credit card extracted from the form. It refers to the
  // CVC-only save. If card is unknown we will offer to save the the card
  // including it's CVC. `credit_card_import_type` is the credit card type
  // extracted from the form.
  // TODO(crbug.com/40270301): Update param after resolving duplicate local and
  // server card issue.
  virtual bool ShouldOfferCvcSave(
      const CreditCard& card,
      FormDataImporter::CreditCardImportType credit_card_import_type,
      bool is_credit_card_upstream_enabled);

  // Check and attempt to offer if CVC or card local or upload save should be
  // offered to the user. `card` is the credit card extracted from the form.
  // `credit_card_import_type` is the credit card type extracted from the form.
  virtual bool ProceedWithSavingIfApplicable(
      const FormStructure& submitted_form,
      const CreditCard& card,
      FormDataImporter::CreditCardImportType credit_card_import_type,
      bool is_credit_card_upstream_enabled);

  // Begins the process to offer upload credit card save to the user if the
  // imported card passes all requirements and Google Payments approves.
  // If |uploading_local_card| is true, the card being
  // offered for upload is already a local card on the device.
  void AttemptToOfferCardUploadSave(const FormStructure& submitted_form,
                                    const CreditCard& card,
                                    const bool uploading_local_card);

  // Begins the process to offer server CVC save to the user.
  virtual void AttemptToOfferCvcUploadSave(const CreditCard& card);

  // Returns true if all the conditions for enabling the upload of credit card
  // are satisfied.
  virtual bool IsCreditCardUploadEnabled();

  // For testing.
  void SetAppLocale(std::string app_locale) { app_locale_ = app_locale; }

  // Set Autofill address profiles that are only preliminarily imported.
  // A preliminary import may happen when the address is found in the same
  // form as a credit card that is currently processed by the manager.
  void SetPreliminarilyImportedAutofillProfile(
      const std::vector<AutofillProfile>& profiles) {
    preliminarily_imported_address_profiles_ = profiles;
  }

  // Clear the preliminarily imported Autofill address profiles.
  void ClearPreliminarilyImportedAutofillProfile() {
    preliminarily_imported_address_profiles_.clear();
  }

 protected:
  // Returns the result of an upload request. If |result| ==
  // |PaymentsRpcResult::kSuccess|, clears strikes for the saved card.
  // Additionally, |server_id| may, optionally, contain the opaque identifier
  // for the card on the server. Exposed for testing.
  virtual void OnDidUploadCard(
      payments::PaymentsAutofillClient::PaymentsRpcResult result,
      const payments::PaymentsNetworkInterface::UploadCardResponseDetails&
          upload_card_response_details);

 private:
  friend class CreditCardSaveManagerTest;
  friend class CreditCardSaveManagerTestObserverBridge;
  friend class LocalCardMigrationBrowserTest;
  friend class TestCreditCardSaveManager;
  friend class SaveCardBubbleViewsFullFormBrowserTest;
  friend class FakeCreditCardServer;
  friend class ::SaveCardOfferObserver;
  FRIEND_TEST_ALL_PREFIXES(
      SaveCardBubbleViewsFullFormBrowserTestWithAutofillUpstream,
      StrikeDatabase_Upload_FullFlowTest);
  FRIEND_TEST_ALL_PREFIXES(SaveCardBubbleViewsFullFormBrowserTest,
                           StrikeDatabase_Local_FullFlowTest);
  FRIEND_TEST_ALL_PREFIXES(SaveCardBubbleViewsFullFormBrowserTestForStatusChip,
                           Feedback_CardSavingAnimation);

  // Starts upstream virtual card enrollment flow. Hides save card confirmation
  // prompt before showing virtual card enrollment prompt.
  void InitVirtualCardEnroll(
      const CreditCard& credit_card,
      std::optional<payments::PaymentsNetworkInterface::
                        GetDetailsForEnrollmentResponseDetails>
          get_details_for_enrollment_response_details);

  // Returns the CreditCardSaveStrikeDatabase for |client_|.
  CreditCardSaveStrikeDatabase* GetCreditCardSaveStrikeDatabase();

  // Returns the CvcStorageStrikeDatabase for `client_`.
  CvcStorageStrikeDatabase* GetCvcStorageStrikeDatabase();

  // Query the CvcStorageStrikeDatabase to check if the offer-to-save prompt for
  // this CVC should be blocked.
  bool DetermineAndLogCvcSaveStrikeDatabaseBlockDecision();

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // Returns the GetLocalCardMigrationStrikeDatabase for |client_|.
  LocalCardMigrationStrikeDatabase* GetLocalCardMigrationStrikeDatabase();
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  // Returns the legal message retrieved from Payments. On failure or not
  // meeting Payments's conditions for upload, |legal_message| will contain
  // nullptr. |supported_card_bin_ranges| is a list of BIN prefix ranges which
  // are supported, with the first and second number in the pair being the start
  // and end of the range.
  void OnDidGetUploadDetails(
      payments::PaymentsAutofillClient::PaymentsRpcResult result,
      const std::u16string& context_token,
      std::unique_ptr<base::Value::Dict> legal_message,
      std::vector<std::pair<int, int>> supported_card_bin_ranges);

  // Logs the number of strikes that a card had when save succeeded.
  void LogStrikesPresentWhenCardSaved(bool is_local, const int num_strikes);

  // Examines |card| and the stored profiles and if a candidate set of profiles
  // is found that matches the client-side validation rules, assigns the values
  // to |upload_request.profiles|. If any problems are found when determining
  // the candidate set of profiles, sets |upload_decision_metrics_| with the
  // failure reasons. Appends any experiments that were triggered to
  // |upload_request.active_experiments|. Note that if the relevant feature is
  // enabled, the addresses being assigned to |upload_request.profiles| may only
  // contain countries.
  void SetProfilesForCreditCardUpload(
      const CreditCard& card,
      payments::PaymentsNetworkInterface::UploadCardRequestDetails*
          upload_request);

  // Analyzes the decisions made while importing address profile and credit card
  // data in preparation for upload credit card save, in order to determine what
  // uploadable data is actually available.
  int GetDetectedValues() const;

  // Offers local credit card save once the Autofill StrikeSystem has made its
  // decision.
  void OfferCardLocalSave();

  // Offers local CVC save once `AttemptToOfferCvcLocalSave()` decides it should
  // be allowed.
  void OfferCvcLocalSave();

  // Offers credit card upload if Payments has allowed offering to save and the
  // Autofill StrikeSystem has made its decision.
  void OfferCardUploadSave();

  // Called once the user makes a decision with respect to the local credit card
  // offer-to-save prompt. If accepted, clears strikes for the to-be-saved card
  // and has `PaymentsDataManager` save the card.
  void OnUserDidDecideOnLocalSave(
      payments::PaymentsAutofillClient::SaveCardOfferUserDecision
          user_decision);

  // Called once the user makes a decision with respect to the local CVC
  // offer-to-save prompt.
  void OnUserDidDecideOnCvcLocalSave(
      payments::PaymentsAutofillClient::SaveCardOfferUserDecision
          user_decision);

  // Called once the user makes a decision with respect to the credit card
  // upload offer-to-save prompt.
  // If accepted:
  //   Sets |user_did_accept_upload_prompt_| and calls SendUploadCardRequest if
  //   the risk data is available. Sets the cardholder name on the upload
  //   request if |user_provided_card_details.cardholder_name| is set. Sets the
  //   expiration date on the upload request if
  //   |user_provided_card_details.expiration_date_month| and
  //   |user_provided_card_details.expiration_date_year| are both set.
  // If rejected or ignored:
  //   Logs a strike against the current card to deter future offers to save.
  void OnUserDidDecideOnUploadSave(
      payments::PaymentsAutofillClient::SaveCardOfferUserDecision user_decision,
      const payments::PaymentsAutofillClient::UserProvidedCardDetails&
          user_provided_card_details);

  // Called once the user makes a decision with respect to the server CVC
  // offer-to-save prompt.
  void OnUserDidDecideOnCvcUploadSave(
      payments::PaymentsAutofillClient::SaveCardOfferUserDecision user_decision,
      const payments::PaymentsAutofillClient::UserProvidedCardDetails&
          user_provided_card_details);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // Upload the card details with the user provided cardholder_name.
  // Only relevant for mobile as fix flow is two steps on mobile compared to
  // one step on desktop.
  void OnUserDidAcceptAccountNameFixFlow(const std::u16string& cardholder_name);

  // Upload the card details with the user provided expiration date month and
  // year. Only relevant for mobile as fix flow is two steps on mobile compared
  // to one step on desktop.
  void OnUserDidAcceptExpirationDateFixFlow(const std::u16string& month,
                                            const std::u16string& year);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

  // Helper function that calls SendUploadCardRequest by setting
  // UserProvidedCardDetails.
  void OnUserDidAcceptUploadHelper(
      const payments::PaymentsAutofillClient::UserProvidedCardDetails&
          user_provided_card_details);

  // Saves risk data in |uploading_risk_data_| and calls SendUploadCardRequest
  // if the user has accepted the prompt.
  void OnDidGetUploadRiskData(const std::string& risk_data);

  // Finalizes the upload request and calls
  // PaymentsNetworkInterface::UploadCard().
  void SendUploadCardRequest();

  // Called when the user ignored or declined the credit card save prompt. Logs
  // a strike for the given card in order to help deter future offers to save,
  // provided that save was actually offered to the user.
  void OnUserDidIgnoreOrDeclineSave(
      const std::u16string& card_last_four_digits);

  // Used for browsertests. Gives the |observer_for_testing_| a notification
  // a strike change has been made.
  void OnStrikeChangeComplete(const int num_strikes);

  // Returns metric relevant to the CVC field based on values in
  // |found_cvc_field_|, |found_value_in_cvc_field_| and
  // |found_cvc_value_in_non_cvc_field_|. Only called when a valid CVC was NOT
  // found.
  autofill_metrics::CardUploadDecision GetCVCCardUploadDecisionMetric() const;

  // Logs the card upload decisions in UKM and UMA.
  // |upload_decision_metrics| is a bitmask of
  // |AutofillMetrics::CardUploadDecisionMetric|.
  void LogCardUploadDecisions(int upload_decision_metrics);

  // Logs the card upload decisions bitmask to chrome://autofill-internals.
  void LogCardUploadDecisionsToAutofillInternals(int upload_decision_metrics);

  // Logs the reason why expiration date was explicitly requested.
  void LogSaveCardRequestExpirationDateReasonMetric();

  // For testing.
  void SetEventObserverForTesting(ObserverForTest* observer) {
    observer_for_testing_ = observer;
  }

  PaymentsDataManager& payments_data_manager();
  const PaymentsDataManager& payments_data_manager() const;

  const raw_ref<AutofillClient> client_;

  std::string app_locale_;

  // The credit card to be saved if local credit card or local or server CVC
  // save is accepted.
  CreditCard card_save_candidate_;

  // Collected information about a pending upload request.
  payments::PaymentsNetworkInterface::UploadCardRequestDetails upload_request_;

  // A bitmask of |AutofillMetrics::CardUploadDecisionMetric| representing the
  // decisions made when determining if credit card upload save should be
  // offered.
  int upload_decision_metrics_ = 0;

  // |true| if the offer-to-save bubble/infobar should pop-up, |false| if not.
  // Will be std::nullopt until data has been retrieved from the StrikeSystem.
  std::optional<bool> show_save_prompt_;

  // |true| if the card being offered for upload is already a local card on the
  // device; |false| otherwise.
  bool uploading_local_card_ = false;

  // |true| if the user has opted to upload save their credit card to Google.
  bool user_did_accept_upload_prompt_ = false;

  // |should_request_expiration_date_from_user_| is |true| if the upload save
  // dialog should request expiration date from the user.
  bool should_request_expiration_date_from_user_ = false;

  // |should_request_name_from_user_| is |true| if the upload save dialog should
  // request cardholder name from the user (prefilled with Google Account name).
  bool should_request_name_from_user_ = false;

  // |found_cvc_field_| is |true| if there exists a field that is determined to
  // be a CVC field via heuristics.
  bool found_cvc_field_ = false;
  // |found_value_in_cvc_field_| is |true| if a field that is determined to
  // be a CVC field via heuristics has non-empty |value|.
  // |value| may or may not be a valid CVC.
  bool found_value_in_cvc_field_ = false;
  // |found_cvc_value_in_non_cvc_field_| is |true| if a field that is not
  // determined to be a CVC field via heuristics has a valid CVC |value|.
  bool found_cvc_value_in_non_cvc_field_ = false;

  // The origin of the top level frame from which a form is uploaded.
  url::Origin pending_upload_request_origin_;

  // The parsed lines from the legal message returned from GetUploadDetails.
  LegalMessageLines legal_message_lines_;

  std::unique_ptr<CreditCardSaveStrikeDatabase>
      credit_card_save_strike_database_;

  std::unique_ptr<CvcStorageStrikeDatabase> cvc_storage_strike_database_;

  // Profiles that are only preliminarily imported. Those profiles are used
  // during a card import to determine the name and country for storing a new
  // card.
  std::vector<AutofillProfile> preliminarily_imported_address_profiles_;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  std::unique_ptr<LocalCardMigrationStrikeDatabase>
      local_card_migration_strike_database_;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  // May be null.
  raw_ptr<ObserverForTest> observer_for_testing_ = nullptr;

  base::WeakPtrFactory<CreditCardSaveManager> weak_ptr_factory_{this};

  FRIEND_TEST_ALL_PREFIXES(
      CreditCardSaveManagerTest,
      UploadCreditCard_ShouldRequestCardholderName_ResetBetweenConsecutiveSaves);
  FRIEND_TEST_ALL_PREFIXES(
      CreditCardSaveManagerTest,
      UploadCreditCard_ShouldRequestExpirationDate_ResetBetweenConsecutiveSaves);
  FRIEND_TEST_ALL_PREFIXES(
      CreditCardSaveManagerTest,
      UploadCreditCard_WalletSyncTransportEnabled_ShouldNotRequestExpirationDate);
  FRIEND_TEST_ALL_PREFIXES(
      CreditCardSaveManagerTest,
      UploadCreditCard_WalletSyncTransportNotEnabled_ShouldRequestExpirationDate);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_SAVE_MANAGER_H_
