// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_CLIENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_CLIENT_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/logging/log_router.h"
#include "components/autofill/core/browser/logging/text_log_receiver.h"
#include "components/autofill/core/browser/mock_autocomplete_history_manager.h"
#include "components/autofill/core/browser/mock_iban_manager.h"
#include "components/autofill/core/browser/mock_merchant_promo_code_manager.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/payments/credit_card_cvc_authenticator.h"
#include "components/autofill/core/browser/payments/credit_card_otp_authenticator.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/test_payments_client.h"
#include "components/autofill/core/browser/payments/test_strike_database.h"
#include "components/autofill/core/browser/test_address_normalizer.h"
#include "components/autofill/core/browser/test_form_data_importer.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_options.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/mock_translate_driver.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/version_info/channel.h"
#include "services/metrics/public/cpp/delegating_ukm_recorder.h"

#if !BUILDFLAG(IS_IOS)
#include "components/webauthn/core/browser/internal_authenticator.h"
#endif

namespace autofill {

// This class is for easier writing of tests.
//
// If you pass the command-line flag --show-autofill-internals,
// autofill-internals logs are recorded to LOG(INFO).
class TestAutofillClient : public AutofillClient {
 public:
  explicit TestAutofillClient(
      std::unique_ptr<TestPersonalDataManager> pdm = nullptr);

  TestAutofillClient(const TestAutofillClient&) = delete;
  TestAutofillClient& operator=(const TestAutofillClient&) = delete;

  ~TestAutofillClient() override;

  // AutofillClient:
  version_info::Channel GetChannel() const override;
  TestPersonalDataManager* GetPersonalDataManager() override;
  AutocompleteHistoryManager* GetAutocompleteHistoryManager() override;
  IBANManager* GetIBANManager() override;
  MerchantPromoCodeManager* GetMerchantPromoCodeManager() override;
  CreditCardCVCAuthenticator* GetCVCAuthenticator() override;
  CreditCardOtpAuthenticator* GetOtpAuthenticator() override;
  PrefService* GetPrefs() override;
  const PrefService* GetPrefs() const override;
  syncer::SyncService* GetSyncService() override;
  signin::IdentityManager* GetIdentityManager() override;
  FormDataImporter* GetFormDataImporter() override;
  payments::PaymentsClient* GetPaymentsClient() override;
  StrikeDatabase* GetStrikeDatabase() override;
  ukm::UkmRecorder* GetUkmRecorder() override;
  ukm::SourceId GetUkmSourceId() override;
  AddressNormalizer* GetAddressNormalizer() override;
  AutofillOfferManager* GetAutofillOfferManager() override;
  const GURL& GetLastCommittedPrimaryMainFrameURL() const override;
  url::Origin GetLastCommittedPrimaryMainFrameOrigin() const override;
  security_state::SecurityLevel GetSecurityLevelForUmaHistograms() override;
  translate::LanguageState* GetLanguageState() override;
  translate::TranslateDriver* GetTranslateDriver() override;
  std::string GetVariationConfigCountryCode() const override;
#if !BUILDFLAG(IS_IOS)
  std::unique_ptr<webauthn::InternalAuthenticator>
  CreateCreditCardInternalAuthenticator(AutofillDriver* driver) override;
#endif

  void ShowAutofillSettings(bool show_credit_card_settings) override;
  void ShowUnmaskPrompt(
      const autofill::CreditCard& card,
      const autofill::CardUnmaskPromptOptions& card_unmask_prompt_options,
      base::WeakPtr<autofill::CardUnmaskDelegate> delegate) override;
  void OnUnmaskVerificationResult(PaymentsRpcResult result) override;
  VirtualCardEnrollmentManager* GetVirtualCardEnrollmentManager() override;
  void ShowVirtualCardEnrollDialog(
      const VirtualCardEnrollmentFields& virtual_card_enrollment_fields,
      base::OnceClosure accept_virtual_card_callback,
      base::OnceClosure decline_virtual_card_callback) override;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  std::vector<std::string> GetAllowedMerchantsForVirtualCards() override;
  std::vector<std::string> GetAllowedBinRangesForVirtualCards() override;

  void ShowLocalCardMigrationDialog(
      base::OnceClosure show_migration_dialog_closure) override;
  void ConfirmMigrateLocalCardToCloud(
      const LegalMessageLines& legal_message_lines,
      const std::string& user_email,
      const std::vector<MigratableCreditCard>& migratable_credit_cards,
      LocalCardMigrationCallback start_migrating_cards_callback) override;
  void ShowLocalCardMigrationResults(
      const bool has_server_error,
      const std::u16string& tip_message,
      const std::vector<MigratableCreditCard>& migratable_credit_cards,
      MigrationDeleteCardCallback delete_local_card_callback) override;
  void ConfirmSaveIBANLocally(const IBAN& iban,
                              bool should_show_prompt,
                              LocalSaveIBANPromptCallback callback) override;
  void ShowWebauthnOfferDialog(
      WebauthnDialogCallback offer_dialog_callback) override;
  void ShowWebauthnVerifyPendingDialog(
      WebauthnDialogCallback verify_pending_dialog_callback) override;
  void UpdateWebauthnOfferDialogWithError() override;
  bool CloseWebauthnDialog() override;
  void ConfirmSaveUpiIdLocally(
      const std::string& upi_id,
      base::OnceCallback<void(bool accept)> callback) override;
  void OfferVirtualCardOptions(
      const std::vector<CreditCard*>& candidates,
      base::OnceCallback<void(const std::string&)> callback) override;
#else  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  void ConfirmAccountNameFixFlow(
      base::OnceCallback<void(const std::u16string&)> callback) override;
  void ConfirmExpirationDateFixFlow(
      const CreditCard& card,
      base::OnceCallback<void(const std::u16string&, const std::u16string&)>
          callback) override;
#endif
  void ConfirmSaveCreditCardLocally(
      const CreditCard& card,
      SaveCreditCardOptions options,
      LocalSaveCardPromptCallback callback) override;

  void ConfirmSaveCreditCardToCloud(
      const CreditCard& card,
      const LegalMessageLines& legal_message_lines,
      SaveCreditCardOptions options,
      UploadSaveCardPromptCallback callback) override;
  void CreditCardUploadCompleted(bool card_saved) override;
  void ConfirmCreditCardFillAssist(const CreditCard& card,
                                   base::OnceClosure callback) override;
  void ConfirmSaveAddressProfile(
      const AutofillProfile& profile,
      const AutofillProfile* original_profile,
      SaveAddressProfilePromptOptions options,
      AddressProfileSavePromptCallback callback) override;
  bool HasCreditCardScanFeature() override;
  void ScanCreditCard(CreditCardScanCallback callback) override;
  bool IsFastCheckoutSupported() override;
  bool IsFastCheckoutTriggerForm(const FormData& form,
                                 const FormFieldData& field) override;
  bool ShowFastCheckout(base::WeakPtr<FastCheckoutDelegate> delegate) override;
  void HideFastCheckout() override;
  bool IsTouchToFillCreditCardSupported() override;
  bool ShowTouchToFillCreditCard(
      base::WeakPtr<TouchToFillDelegate> delegate,
      base::span<const autofill::CreditCard* const> cards_to_suggest) override;
  void HideTouchToFillCreditCard() override;
  void ShowAutofillPopup(
      const AutofillClient::PopupOpenArgs& open_args,
      base::WeakPtr<AutofillPopupDelegate> delegate) override;
  void UpdateAutofillPopupDataListValues(
      const std::vector<std::u16string>& values,
      const std::vector<std::u16string>& labels) override;
  base::span<const Suggestion> GetPopupSuggestions() const override;
  void PinPopupView() override;
  AutofillClient::PopupOpenArgs GetReopenPopupArgs() const override;
  void UpdatePopup(const std::vector<Suggestion>& suggestions,
                   PopupType popup_type) override;
  void HideAutofillPopup(PopupHidingReason reason) override;
  void ShowVirtualCardErrorDialog(
      const AutofillErrorDialogContext& context) override;
  bool IsAutocompleteEnabled() const override;
  bool IsPasswordManagerEnabled() override;
  void PropagateAutofillPredictions(
      AutofillDriver* driver,
      const std::vector<FormStructure*>& forms) override;
  void DidFillOrPreviewField(const std::u16string& autofilled_value,
                             const std::u16string& profile_full_name) override;
  // By default, TestAutofillClient will report that the context is
  // secure. This can be adjusted by calling set_form_origin() with an
  // http:// URL.
  bool IsContextSecure() const override;
  bool ShouldShowSigninPromo() override;
  bool AreServerCardsSupported() const override;
  void ExecuteCommand(int id) override;
  void OpenPromoCodeOfferDetailsURL(const GURL& url) override;
  LogManager* GetLogManager() const override;
  FormInteractionsFlowId GetCurrentFormInteractionsFlowId() override;

  // RiskDataLoader:
  void LoadRiskData(
      base::OnceCallback<void(const std::string&)> callback) override;

#if BUILDFLAG(IS_IOS)
  bool IsLastQueriedField(FieldGlobalId field_id) override;
#endif

  // Initializes UKM source from form_origin_. This needs to be called
  // in unittests after calling Purge for ukm recorder to re-initialize
  // sources.
  void InitializeUKMSources();

  void SetPrefs(std::unique_ptr<PrefService> prefs) {
    prefs_ = std::move(prefs);
  }

  void set_personal_data_manager(std::unique_ptr<TestPersonalDataManager> pdm) {
    test_personal_data_manager_ = std::move(pdm);
  }

  void set_cvc_authenticator(
      std::unique_ptr<CreditCardCVCAuthenticator> authenticator) {
    cvc_authenticator_ = std::move(authenticator);
  }

  void set_otp_authenticator(
      std::unique_ptr<CreditCardOtpAuthenticator> authenticator) {
    otp_authenticator_ = std::move(authenticator);
  }

  void set_test_strike_database(
      std::unique_ptr<TestStrikeDatabase> test_strike_database) {
    test_strike_database_ = std::move(test_strike_database);
  }

  void set_test_payments_client(
      std::unique_ptr<payments::TestPaymentsClient> payments_client) {
    payments_client_ = std::move(payments_client);
  }

  void set_test_form_data_importer(
      std::unique_ptr<FormDataImporter> form_data_importer) {
    form_data_importer_ = std::move(form_data_importer);
  }

  void set_form_origin(const GURL& url);

  void set_sync_service(syncer::SyncService* test_sync_service) {
    test_sync_service_ = test_sync_service;
  }

  void set_security_level(security_state::SecurityLevel security_level) {
    security_level_ = security_level;
  }

  void set_last_committed_primary_main_frame_url(const GURL& url);

  void SetVariationConfigCountryCode(
      const std::string& variation_config_country_code) {
    variation_config_country_code_ = variation_config_country_code;
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  void set_allowed_merchants(
      const std::vector<std::string>& merchant_allowlist) {
    allowed_merchants_ = merchant_allowlist;
  }

  void set_allowed_bin_ranges(
      const std::vector<std::string>& bin_range_allowlist) {
    allowed_bin_ranges_ = bin_range_allowlist;
  }
#endif

  void set_should_save_autofill_profiles(bool value) {
    should_save_autofill_profiles_ = value;
  }

  void Reset() {
    confirm_save_iban_locally_called_ = false;
    offer_to_save_iban_bubble_was_shown_ = false;
  }

  bool ConfirmSaveCardLocallyWasCalled() {
    return confirm_save_credit_card_locally_called_;
  }

  bool ConfirmSaveIBANLocallyWasCalled() {
    return confirm_save_iban_locally_called_;
  }

  bool offer_to_save_iban_bubble_was_shown() {
    return offer_to_save_iban_bubble_was_shown_;
  }

  bool get_offer_to_save_credit_card_bubble_was_shown() {
    return offer_to_save_credit_card_bubble_was_shown_.value();
  }

  void set_virtual_card_error_dialog_shown(
      bool virtual_card_error_dialog_shown) {
    virtual_card_error_dialog_shown_ = virtual_card_error_dialog_shown;
  }

  bool virtual_card_error_dialog_shown() {
    return virtual_card_error_dialog_shown_;
  }

  bool virtual_card_error_dialog_is_permanent_error() {
    return autofill_error_dialog_context().type ==
           AutofillErrorDialogType::kVirtualCardPermanentError;
  }

  AutofillErrorDialogContext autofill_error_dialog_context() {
    return autofill_error_dialog_context_;
  }

  SaveCreditCardOptions get_save_credit_card_options() {
    return save_credit_card_options_.value();
  }

  ::testing::NiceMock<MockAutocompleteHistoryManager>*
  GetMockAutocompleteHistoryManager() {
    return &mock_autocomplete_history_manager_;
  }

  ::testing::NiceMock<MockIBANManager>* GetMockIBANManager() {
    return mock_iban_manager_.get();
  }

  ::testing::NiceMock<MockMerchantPromoCodeManager>*
  GetMockMerchantPromoCodeManager() {
    return &mock_merchant_promo_code_manager_;
  }

  void set_migration_card_selections(
      const std::vector<std::string>& migration_card_selection) {
    migration_card_selection_ = migration_card_selection;
  }

  void set_autofill_offer_manager(
      std::unique_ptr<AutofillOfferManager> autofill_offer_manager) {
    autofill_offer_manager_ = std::move(autofill_offer_manager);
  }

  void set_channel_for_testing(const version_info::Channel channel) {
    channel_for_testing_ = channel;
  }

  GURL form_origin() { return form_origin_; }

  ukm::TestUkmRecorder* GetTestUkmRecorder();

 private:
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
  signin::IdentityTestEnvironment identity_test_env_;
  raw_ptr<syncer::SyncService> test_sync_service_ = nullptr;
  TestAddressNormalizer test_address_normalizer_;
  ::testing::NiceMock<MockAutocompleteHistoryManager>
      mock_autocomplete_history_manager_;
  std::unique_ptr<testing::NiceMock<MockIBANManager>> mock_iban_manager_;
  ::testing::NiceMock<MockMerchantPromoCodeManager>
      mock_merchant_promo_code_manager_;

  // NULL by default.
  std::unique_ptr<PrefService> prefs_;
  std::unique_ptr<TestStrikeDatabase> test_strike_database_;
  std::unique_ptr<payments::PaymentsClient> payments_client_;
  std::unique_ptr<CreditCardCVCAuthenticator> cvc_authenticator_;
  std::unique_ptr<CreditCardOtpAuthenticator> otp_authenticator_;

  // AutofillOfferManager and TestFormDataImporter must be destroyed before
  // TestPersonalDataManager, because the former's destructors refer to the
  // latter.
  std::unique_ptr<TestPersonalDataManager> test_personal_data_manager_;
  std::unique_ptr<AutofillOfferManager> autofill_offer_manager_;
  std::unique_ptr<FormDataImporter> form_data_importer_;

  GURL form_origin_;
  ukm::SourceId source_id_ = -1;
  std::string variation_config_country_code_;

  security_state::SecurityLevel security_level_ =
      security_state::SecurityLevel::NONE;

  bool should_save_autofill_profiles_ = true;

  bool confirm_save_credit_card_locally_called_ = false;

  bool confirm_save_iban_locally_called_ = false;

  bool virtual_card_error_dialog_shown_ = false;

  // Context parameters that are used to display an error dialog during card
  // number retrieval. This context will have information that the autofill
  // error dialog uses to display a dialog specific to the error that occurred.
  // An example of where this dialog is used is if an error occurs during
  // virtual card number retrieval, as this context is then filled with fields
  // specific to the type of error that occurred, and then based on the contents
  // of this context the dialog is shown.
  AutofillErrorDialogContext autofill_error_dialog_context_;

  // Populated if save was offered. True if bubble was shown, false otherwise.
  absl::optional<bool> offer_to_save_credit_card_bubble_was_shown_;

  // Populated if name fix flow was offered. True if bubble was shown, false
  // otherwise.
  absl::optional<bool> credit_card_name_fix_flow_bubble_was_shown_;

  version_info::Channel channel_for_testing_ = version_info::Channel::UNKNOWN;

  // Populated if credit card local save or upload was offered.
  absl::optional<SaveCreditCardOptions> save_credit_card_options_;

  // Populated if IBAN save was offered. True if bubble was shown, false
  // otherwise.
  bool offer_to_save_iban_bubble_was_shown_ = false;

  std::vector<std::string> migration_card_selection_;

  // A mock translate driver which provides the language state.
  translate::testing::MockTranslateDriver mock_translate_driver_;

  // The last URL submitted in the primary main frame by the user. Set in the
  // constructor.
  GURL last_committed_primary_main_frame_url_;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  std::vector<std::string> allowed_merchants_;
  std::vector<std::string> allowed_bin_ranges_;
#endif

  LogRouter log_router_;
  std::unique_ptr<LogManager> log_manager_;
  TextLogReceiver text_log_receiver_;
  base::ScopedObservation<LogRouter, LogReceiver> scoped_logging_subscription_{
      &text_log_receiver_};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_CLIENT_H_
