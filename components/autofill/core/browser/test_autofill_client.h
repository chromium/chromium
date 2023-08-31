// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_CLIENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_CLIENT_H_

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_download_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/logging/log_router.h"
#include "components/autofill/core/browser/logging/text_log_receiver.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/mock_autocomplete_history_manager.h"
#include "components/autofill/core/browser/mock_autofill_optimization_guide.h"
#include "components/autofill/core/browser/mock_iban_manager.h"
#include "components/autofill/core/browser/mock_merchant_promo_code_manager.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/payments/credit_card_cvc_authenticator.h"
#include "components/autofill/core/browser/payments/credit_card_otp_authenticator.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/local_card_migration_manager.h"
#include "components/autofill/core/browser/payments/test/mock_mandatory_reauth_manager.h"
#include "components/autofill/core/browser/payments/test_payments_client.h"
#include "components/autofill/core/browser/strike_databases/payments/test_strike_database.h"
#include "components/autofill/core/browser/test_address_normalizer.h"
#include "components/autofill/core/browser/test_form_data_importer.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/ui/mock_fast_checkout_client.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_options.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/popup_types.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/mock_translate_driver.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/version_info/channel.h"
#include "services/metrics/public/cpp/delegating_ukm_recorder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"

#if !BUILDFLAG(IS_IOS)
#include "components/autofill/core/browser/payments/test_internal_authenticator.h"
#include "components/webauthn/core/browser/internal_authenticator.h"
#endif

namespace autofill {

// This class is for easier writing of tests. There are two instances of the
// template:
//
// - TestAutofillClient is a simple AutofillClient;
// - TestContentAutofillClient is a ContentAutofillClient, i.e., is associated
//   to a content::WebContents and has a ContentAutofillDriverFactory
//
// As a rule of thumb, TestContentAutofillClient is preferable in tests that
// have a content::WebContents.
//
// If you enable the Finch feature `kAutofillLoggingToTerminal`,
// autofill-internals logs are recorded to LOG(INFO).
template <typename T>
class TestAutofillClientTemplate : public T {
 public:
  static_assert(std::is_base_of_v<AutofillClient, T>);

  using T::T;
  TestAutofillClientTemplate(const TestAutofillClientTemplate&) = delete;
  TestAutofillClientTemplate& operator=(const TestAutofillClientTemplate&) =
      delete;
  ~TestAutofillClientTemplate() override = default;

  // Initializes UKM source from form_origin_. This needs to be called
  // in unittests after calling Purge for ukm recorder to re-initialize
  // sources.
  void InitializeUKMSources() {
    test_ukm_recorder_.UpdateSourceURL(source_id_, form_origin_);
  }

  version_info::Channel GetChannel() const override {
    return channel_for_testing_;
  }

  bool IsOffTheRecord() override { return is_off_the_record_; }

  AutofillDownloadManager* GetDownloadManager() override {
    return download_manager_.get();
  }

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    return test_shared_loader_factory_;
  }

  TestPersonalDataManager* GetPersonalDataManager() override {
    if (!test_personal_data_manager_) {
      test_personal_data_manager_ = std::make_unique<TestPersonalDataManager>();
    }
    return test_personal_data_manager_.get();
  }

  AutofillOptimizationGuide* GetAutofillOptimizationGuide() const override {
    return mock_autofill_optimization_guide_.get();
  }

  void ResetAutofillOptimizationGuide() {
    mock_autofill_optimization_guide_.reset();
  }

  AutocompleteHistoryManager* GetAutocompleteHistoryManager() override {
    return &mock_autocomplete_history_manager_;
  }

  IbanManager* GetIbanManager() override { return GetMockIbanManager(); }

  plus_addresses::PlusAddressService* GetPlusAddressService() override {
    return test_plus_address_service_;
  }

  MerchantPromoCodeManager* GetMerchantPromoCodeManager() override {
    return &mock_merchant_promo_code_manager_;
  }

  CreditCardCvcAuthenticator* GetCvcAuthenticator() override {
    if (!cvc_authenticator_) {
      cvc_authenticator_ = std::make_unique<CreditCardCvcAuthenticator>(this);
    }
    return cvc_authenticator_.get();
  }

  CreditCardOtpAuthenticator* GetOtpAuthenticator() override {
    if (!otp_authenticator_) {
      otp_authenticator_ = std::make_unique<CreditCardOtpAuthenticator>(this);
    }
    return otp_authenticator_.get();
  }

  PrefService* GetPrefs() override {
    if (!prefs_) {
      prefs_ = autofill::test::PrefServiceForTesting();
    }
    return prefs_.get();
  }

  const PrefService* GetPrefs() const override {
    return const_cast<TestAutofillClientTemplate*>(this)->GetPrefs();
  }

  syncer::SyncService* GetSyncService() override { return test_sync_service_; }

  signin::IdentityManager* GetIdentityManager() override {
    return identity_test_env_.identity_manager();
  }

  FormDataImporter* GetFormDataImporter() override {
    if (!form_data_importer_) {
      set_test_form_data_importer(std::make_unique<FormDataImporter>(
          /*client=*/this, /*payments_client=*/nullptr,
          /*personal_data_manager=*/nullptr, /*app_locale=*/"en-US"));
    }

    return form_data_importer_.get();
  }

  payments::PaymentsClient* GetPaymentsClient() override {
    return payments_client_.get();
  }

  StrikeDatabase* GetStrikeDatabase() override {
    return test_strike_database_.get();
  }

  ukm::UkmRecorder* GetUkmRecorder() override { return &test_ukm_recorder_; }

  ukm::SourceId GetUkmSourceId() override {
    if (source_id_ == -1) {
      source_id_ = ukm::UkmRecorder::GetNewSourceID();
      test_ukm_recorder_.UpdateSourceURL(source_id_, form_origin_);
    }
    return source_id_;
  }

  AddressNormalizer* GetAddressNormalizer() override {
    return &test_address_normalizer_;
  }

  AutofillOfferManager* GetAutofillOfferManager() override {
    return autofill_offer_manager_.get();
  }

  FastCheckoutClient* GetFastCheckoutClient() override {
    return &mock_fast_checkout_client_;
  }

  const GURL& GetLastCommittedPrimaryMainFrameURL() const override {
    return last_committed_primary_main_frame_url_;
  }

  url::Origin GetLastCommittedPrimaryMainFrameOrigin() const override {
    return url::Origin::Create(last_committed_primary_main_frame_url_);
  }

  security_state::SecurityLevel GetSecurityLevelForUmaHistograms() override {
    return security_level_;
  }

  translate::LanguageState* GetLanguageState() override {
    return &mock_translate_driver_.GetLanguageState();
  }

  translate::TranslateDriver* GetTranslateDriver() override {
    return &mock_translate_driver_;
  }

  GeoIpCountryCode GetVariationConfigCountryCode() const override {
    return variation_config_country_code_;
  }

#if !BUILDFLAG(IS_IOS)
  std::unique_ptr<webauthn::InternalAuthenticator>
  CreateCreditCardInternalAuthenticator(AutofillDriver* driver) override {
    return std::make_unique<TestInternalAuthenticator>();
  }
#endif

  void ShowAutofillSettings(PopupType popup_type) override {}

  void ShowUnmaskPrompt(
      const autofill::CreditCard& card,
      const autofill::CardUnmaskPromptOptions& card_unmask_prompt_options,
      base::WeakPtr<autofill::CardUnmaskDelegate> delegate) override {}

  void OnUnmaskVerificationResult(
      AutofillClient::PaymentsRpcResult result) override {}

  VirtualCardEnrollmentManager* GetVirtualCardEnrollmentManager() override {
    return form_data_importer_->GetVirtualCardEnrollmentManager();
  }

  void ShowVirtualCardEnrollDialog(
      const VirtualCardEnrollmentFields& virtual_card_enrollment_fields,
      base::OnceClosure accept_virtual_card_callback,
      base::OnceClosure decline_virtual_card_callback) override {}

  payments::MandatoryReauthManager* GetOrCreatePaymentsMandatoryReauthManager()
      override {
    if (!mock_payments_mandatory_reauth_manager_) {
      mock_payments_mandatory_reauth_manager_ = std::make_unique<
          testing::NiceMock<payments::MockMandatoryReauthManager>>();
    }
    return mock_payments_mandatory_reauth_manager_.get();
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  std::vector<std::string> GetAllowedMerchantsForVirtualCards() override {
    return allowed_merchants_;
  }

  std::vector<std::string> GetAllowedBinRangesForVirtualCards() override {
    return allowed_bin_ranges_;
  }

  void ShowLocalCardMigrationDialog(
      base::OnceClosure show_migration_dialog_closure) override {
    std::move(show_migration_dialog_closure).Run();
  }

  void ConfirmMigrateLocalCardToCloud(
      const LegalMessageLines& legal_message_lines,
      const std::string& user_email,
      const std::vector<MigratableCreditCard>& migratable_credit_cards,
      AutofillClient::LocalCardMigrationCallback start_migrating_cards_callback)
      override {
    // If |migration_card_selection_| hasn't been preset by tests, default to
    // selecting all migratable cards.
    if (migration_card_selection_.empty()) {
      for (MigratableCreditCard card : migratable_credit_cards) {
        migration_card_selection_.push_back(card.credit_card().guid());
      }
    }
    std::move(start_migrating_cards_callback).Run(migration_card_selection_);
  }

  void ShowLocalCardMigrationResults(
      const bool has_server_error,
      const std::u16string& tip_message,
      const std::vector<MigratableCreditCard>& migratable_credit_cards,
      AutofillClient::MigrationDeleteCardCallback delete_local_card_callback)
      override {}

  void ConfirmSaveIbanLocally(
      const Iban& iban,
      bool should_show_prompt,
      AutofillClient::LocalSaveIbanPromptCallback callback) override {
    confirm_save_iban_locally_called_ = true;
    offer_to_save_iban_bubble_was_shown_ = should_show_prompt;
  }

  void ShowWebauthnOfferDialog(
      AutofillClient::WebauthnDialogCallback offer_dialog_callback) override {}

  void ShowWebauthnVerifyPendingDialog(
      AutofillClient::WebauthnDialogCallback verify_pending_dialog_callback)
      override {}

  void UpdateWebauthnOfferDialogWithError() override {}

  bool CloseWebauthnDialog() override { return true; }

  void OfferVirtualCardOptions(
      const std::vector<CreditCard*>& candidates,
      base::OnceCallback<void(const std::string&)> callback) override {}

#else  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

  void ConfirmAccountNameFixFlow(
      base::OnceCallback<void(const std::u16string&)> callback) override {
    credit_card_name_fix_flow_bubble_was_shown_ = true;
    std::move(callback).Run(std::u16string(u"Gaia Name"));
  }

  void ConfirmExpirationDateFixFlow(
      const CreditCard& card,
      base::OnceCallback<void(const std::u16string&, const std::u16string&)>
          callback) override {
    credit_card_name_fix_flow_bubble_was_shown_ = true;
    std::move(callback).Run(
        std::u16string(u"03"),
        std::u16string(base::ASCIIToUTF16(test::NextYear().c_str())));
  }

#endif

  void ConfirmSaveCreditCardLocally(
      const CreditCard& card,
      AutofillClient::SaveCreditCardOptions options,
      AutofillClient::LocalSaveCardPromptCallback callback) override {
    confirm_save_credit_card_locally_called_ = true;
    offer_to_save_credit_card_bubble_was_shown_ = options.show_prompt;
    save_credit_card_options_ = options;
    std::move(callback).Run(
        AutofillClient::SaveCardOfferUserDecision::kAccepted);
  }

  void ConfirmSaveCreditCardToCloud(
      const CreditCard& card,
      const LegalMessageLines& legal_message_lines,
      AutofillClient::SaveCreditCardOptions options,
      AutofillClient::UploadSaveCardPromptCallback callback) override {
    offer_to_save_credit_card_bubble_was_shown_ = options.show_prompt;
    save_credit_card_options_ = options;
    std::move(callback).Run(
        AutofillClient::SaveCardOfferUserDecision::kAccepted, {});
  }

  void CreditCardUploadCompleted(bool card_saved) override {}

  void ConfirmCreditCardFillAssist(const CreditCard& card,
                                   base::OnceClosure callback) override {
    std::move(callback).Run();
  }

  void ConfirmSaveAddressProfile(
      const AutofillProfile& profile,
      const AutofillProfile* original_profile,
      AutofillClient::SaveAddressProfilePromptOptions options,
      AutofillClient::AddressProfileSavePromptCallback callback) override {}

  void ShowEditAddressProfileDialog(const AutofillProfile& profile) override {}

  void ShowDeleteAddressProfileDialog() override {}

  bool HasCreditCardScanFeature() override { return false; }

  void ScanCreditCard(
      AutofillClient::CreditCardScanCallback callback) override {}

  bool IsTouchToFillCreditCardSupported() override { return false; }

  bool ShowTouchToFillCreditCard(
      base::WeakPtr<TouchToFillDelegate> delegate,
      base::span<const autofill::CreditCard> cards_to_suggest) override {
    return false;
  }

  void HideTouchToFillCreditCard() override {}

  void ShowAutofillPopup(
      const AutofillClient::PopupOpenArgs& open_args,
      base::WeakPtr<AutofillPopupDelegate> delegate) override {
    is_showing_popup_ = true;
  }

  void UpdateAutofillPopupDataListValues(
      const std::vector<std::u16string>& values,
      const std::vector<std::u16string>& labels) override {}

  std::vector<Suggestion> GetPopupSuggestions() const override { return {}; }

  void PinPopupView() override {}

  AutofillClient::PopupOpenArgs GetReopenPopupArgs(
      AutofillSuggestionTriggerSource trigger_source) const override {
    return {};
  }

  void UpdatePopup(const std::vector<Suggestion>& suggestions,
                   PopupType popup_type,
                   AutofillSuggestionTriggerSource trigger_source) override {}

  void HideAutofillPopup(PopupHidingReason reason) override {
    popup_hidden_reason_ = reason;
    is_showing_popup_ = false;
  }

  bool IsShowingAutofillPopup() { return is_showing_popup_; }

  PopupHidingReason popup_hiding_reason() { return popup_hidden_reason_; }

  void ShowAutofillErrorDialog(
      const AutofillErrorDialogContext& context) override {
    autofill_error_dialog_shown_ = true;
    autofill_error_dialog_context_ = context;
  }

  void CloseAutofillProgressDialog(
      bool show_confirmation_before_closing,
      base::OnceClosure no_user_perceived_authentication_callback) override {
    if (no_user_perceived_authentication_callback) {
      std::move(no_user_perceived_authentication_callback).Run();
    }
  }

  bool IsAutocompleteEnabled() const override { return true; }

  bool IsPasswordManagerEnabled() override { return true; }

  void PropagateAutofillPredictionsDeprecated(
      AutofillDriver* driver,
      const std::vector<FormStructure*>& forms) override {}

  void DidFillOrPreviewForm(mojom::AutofillActionPersistence action_persistence,
                            AutofillTriggerSource trigger_source,
                            bool is_refill) override {}

  void DidFillOrPreviewField(const std::u16string& autofilled_value,
                             const std::u16string& profile_full_name) override {
  }

  bool IsContextSecure() const override {
    // Simplified secure context check for tests.
    return form_origin_.SchemeIs("https");
  }

  void OpenPromoCodeOfferDetailsURL(const GURL& url) override {}

  LogManager* GetLogManager() const override { return log_manager_.get(); }

  FormInteractionsFlowId GetCurrentFormInteractionsFlowId() override {
    return {};
  }

  scoped_refptr<device_reauth::DeviceAuthenticator> GetDeviceAuthenticator()
      const override {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
    return mock_device_authenticator_;
#else
    return nullptr;
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
  }

  void ShowMandatoryReauthOptInPrompt(
      base::OnceClosure accept_mandatory_reauth_callback,
      base::OnceClosure cancel_mandatory_reauth_callback,
      base::RepeatingClosure close_mandatory_reauth_callback) override {
    mandatory_reauth_opt_in_prompt_was_shown_ = true;
  }

  bool GetMandatoryReauthOptInPromptWasShown() {
    return mandatory_reauth_opt_in_prompt_was_shown_;
  }

  void ShowMandatoryReauthOptInConfirmation() override {
    mandatory_reauth_opt_in_prompt_was_reshown_ = true;
  }

  bool GetMandatoryReauthOptInPromptWasReshown() {
    return mandatory_reauth_opt_in_prompt_was_reshown_;
  }

  void LoadRiskData(
      base::OnceCallback<void(const std::string&)> callback) override {
    std::move(callback).Run("some risk data");
  }

#if BUILDFLAG(IS_IOS)
  bool IsLastQueriedField(FieldGlobalId field_id) override { return true; }
#endif

  void SetPrefs(std::unique_ptr<PrefService> prefs) {
    prefs_ = std::move(prefs);
  }

  void set_personal_data_manager(std::unique_ptr<TestPersonalDataManager> pdm) {
    test_personal_data_manager_ = std::move(pdm);
  }

  void set_cvc_authenticator(
      std::unique_ptr<CreditCardCvcAuthenticator> authenticator) {
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

  void set_form_origin(const GURL& url) {
    form_origin_ = url;
    // Also reset source_id_.
    source_id_ = ukm::UkmRecorder::GetNewSourceID();
    test_ukm_recorder_.UpdateSourceURL(source_id_, form_origin_);
  }

  void set_sync_service(syncer::SyncService* test_sync_service) {
    test_sync_service_ = test_sync_service;
  }

  void set_security_level(security_state::SecurityLevel security_level) {
    security_level_ = security_level;
  }

  void set_last_committed_primary_main_frame_url(const GURL& url) {
    last_committed_primary_main_frame_url_ = url;
  }

  void SetVariationConfigCountryCode(
      const GeoIpCountryCode& variation_config_country_code) {
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

  bool ConfirmSaveIbanLocallyWasCalled() {
    return confirm_save_iban_locally_called_;
  }

  bool offer_to_save_iban_bubble_was_shown() {
    return offer_to_save_iban_bubble_was_shown_;
  }

  bool get_offer_to_save_credit_card_bubble_was_shown() {
    return offer_to_save_credit_card_bubble_was_shown_.value();
  }

  void set_autofill_error_dialog_shown(bool autofill_error_dialog_shown) {
    autofill_error_dialog_shown_ = autofill_error_dialog_shown;
  }

  bool autofill_error_dialog_shown() { return autofill_error_dialog_shown_; }

  bool virtual_card_error_dialog_is_permanent_error() {
    return autofill_error_dialog_context().type ==
           AutofillErrorDialogType::kVirtualCardPermanentError;
  }

  AutofillErrorDialogContext autofill_error_dialog_context() {
    return autofill_error_dialog_context_;
  }

  AutofillClient::SaveCreditCardOptions get_save_credit_card_options() {
    return save_credit_card_options_.value();
  }

  ::testing::NiceMock<MockAutocompleteHistoryManager>*
  GetMockAutocompleteHistoryManager() {
    return &mock_autocomplete_history_manager_;
  }

  ::testing::NiceMock<MockIbanManager>* GetMockIbanManager() {
    if (!mock_iban_manager_) {
      mock_iban_manager_ = std::make_unique<testing::NiceMock<MockIbanManager>>(
          test_personal_data_manager_.get());
    }
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

  void set_is_off_the_record(bool is_off_the_record) {
    is_off_the_record_ = is_off_the_record;
  }

  void set_download_manager(
      std::unique_ptr<AutofillDownloadManager> download_manager) {
    download_manager_ = std::move(download_manager);
  }

  void set_shared_url_loader_factory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
    test_shared_loader_factory_ = url_loader_factory;
  }

  void set_plus_address_service(
      plus_addresses::PlusAddressService* plus_address_service) {
    test_plus_address_service_ = plus_address_service;
  }

  GURL form_origin() { return form_origin_; }

  ukm::TestUkmRecorder* GetTestUkmRecorder() { return &test_ukm_recorder_; }

 private:
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
  signin::IdentityTestEnvironment identity_test_env_;
  raw_ptr<syncer::SyncService> test_sync_service_ = nullptr;
  raw_ptr<plus_addresses::PlusAddressService> test_plus_address_service_ =
      nullptr;
  TestAddressNormalizer test_address_normalizer_;
  std::unique_ptr<::testing::NiceMock<MockAutofillOptimizationGuide>>
      mock_autofill_optimization_guide_ =
          std::make_unique<testing::NiceMock<MockAutofillOptimizationGuide>>();
  ::testing::NiceMock<MockAutocompleteHistoryManager>
      mock_autocomplete_history_manager_;
  ::testing::NiceMock<MockMerchantPromoCodeManager>
      mock_merchant_promo_code_manager_;
  ::testing::NiceMock<MockFastCheckoutClient> mock_fast_checkout_client_;
  scoped_refptr<device_reauth::MockDeviceAuthenticator>
      mock_device_authenticator_ =
          base::MakeRefCounted<device_reauth::MockDeviceAuthenticator>();
  std::unique_ptr<::testing::NiceMock<payments::MockMandatoryReauthManager>>
      mock_payments_mandatory_reauth_manager_;

  // NULL by default.
  std::unique_ptr<PrefService> prefs_;
  std::unique_ptr<TestStrikeDatabase> test_strike_database_;

  std::unique_ptr<TestPersonalDataManager> test_personal_data_manager_;
  // The below objects must be destroyed before `TestPersonalDataManager`
  // because they keep a reference to it.
  std::unique_ptr<AutofillOfferManager> autofill_offer_manager_;
  std::unique_ptr<payments::PaymentsClient> payments_client_;
  std::unique_ptr<testing::NiceMock<MockIbanManager>> mock_iban_manager_;

  // The below objects must be destroyed before `PaymentsClient` because they
  // (or their members) keep a reference to it.
  std::unique_ptr<CreditCardCvcAuthenticator> cvc_authenticator_;
  std::unique_ptr<CreditCardOtpAuthenticator> otp_authenticator_;
  std::unique_ptr<FormDataImporter> form_data_importer_;

  GURL form_origin_{"https://example.test"};
  ukm::SourceId source_id_ = -1;
  GeoIpCountryCode variation_config_country_code_;

  security_state::SecurityLevel security_level_ =
      security_state::SecurityLevel::NONE;

  bool should_save_autofill_profiles_ = true;

  bool confirm_save_credit_card_locally_called_ = false;

  bool confirm_save_iban_locally_called_ = false;

  bool autofill_error_dialog_shown_ = false;

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

  bool is_off_the_record_ = false;

  bool is_showing_popup_ = false;

  PopupHidingReason popup_hidden_reason_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);

  std::unique_ptr<AutofillDownloadManager> download_manager_;

  // Populated if credit card local save or upload was offered.
  absl::optional<AutofillClient::SaveCreditCardOptions>
      save_credit_card_options_;

  // Populated if IBAN save was offered. True if bubble was shown, false
  // otherwise.
  bool offer_to_save_iban_bubble_was_shown_ = false;

  // Populated if mandatory re-auth opt-in was offered, or re-offered,
  // respectively.
  bool mandatory_reauth_opt_in_prompt_was_shown_ = false;
  bool mandatory_reauth_opt_in_prompt_was_reshown_ = false;

  std::vector<std::string> migration_card_selection_;

  // A mock translate driver which provides the language state.
  translate::testing::MockTranslateDriver mock_translate_driver_;

  // The last URL submitted in the primary main frame by the user. Set in the
  // constructor.
  GURL last_committed_primary_main_frame_url_{"https://example.test"};

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  std::vector<std::string> allowed_merchants_;
  std::vector<std::string> allowed_bin_ranges_;
#endif

  LogRouter log_router_;
  struct LogToTerminal {
    explicit LogToTerminal(LogRouter& log_router) {
      if (base::FeatureList::IsEnabled(
              features::test::kAutofillLogToTerminal)) {
        log_router.LogToTerminal();
      }
    }
  } log_to_terminal_{log_router_};
  std::unique_ptr<LogManager> log_manager_ =
      LogManager::Create(&log_router_, base::NullCallback());
};

// A simple `AutofillClient` for tests. Consider `TestContentAutofillClient` as
// an alternative for tests where the content layer is visible.
//
// Consider using TestAutofillClientInjector, especially in browser tests.
class TestAutofillClient : public TestAutofillClientTemplate<AutofillClient> {
 public:
  using TestAutofillClientTemplate<AutofillClient>::TestAutofillClientTemplate;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_CLIENT_H_
