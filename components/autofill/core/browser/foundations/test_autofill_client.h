// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_TEST_AUTOFILL_CLIENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_TEST_AUTOFILL_CLIENT_H_

#include <concepts>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/i18n/rtl.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_manager.h"
#include "components/autofill/core/browser/crowdsourcing/mock_autofill_crowdsourcing_manager.h"
#include "components/autofill/core/browser/crowdsourcing/test_votes_uploader.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/data_manager/valuables/test_valuables_data_manager.h"
#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_quality/addresses/test_address_normalizer.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_driver_factory.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver_factory.h"
#include "components/autofill/core/browser/integrators/autofill_ai/mock_autofill_ai_manager.h"
#include "components/autofill/core/browser/integrators/fast_checkout/mock_fast_checkout_client.h"
#include "components/autofill/core/browser/integrators/identity_credential/identity_credential_delegate.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_phish_guard_delegate.h"
#include "components/autofill/core/browser/integrators/optimization_guide/mock_autofill_optimization_guide_decider.h"
#include "components/autofill/core/browser/integrators/password_manager/password_manager_delegate.h"
#include "components/autofill/core/browser/integrators/plus_addresses/autofill_plus_address_delegate.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/logging/log_router.h"
#include "components/autofill/core/browser/logging/text_log_receiver.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/form_interactions_ukm_logger.h"
#include "components/autofill/core/browser/payments/test_payments_autofill_client.h"
#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"
#include "components/autofill/core/browser/single_field_fillers/autocomplete/mock_autocomplete_history_manager.h"
#include "components/autofill/core/browser/single_field_fillers/payments/mock_merchant_promo_code_manager.h"
#include "components/autofill/core/browser/single_field_fillers/single_field_fill_router.h"
#include "components/autofill/core/browser/strike_databases/payments/test_strike_database.h"
#include "components/autofill/core/browser/studies/autofill_ablation_study.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_options.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/autofill/core/browser/webdata/valuables/valuables_table.h"
#include "components/autofill/core/common/autofill_debug_features.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/one_time_tokens/core/browser/one_time_token_service_impl.h"
#include "components/one_time_tokens/core/browser/sms_otp_backend.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/mock_translate_driver.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/version_info/channel.h"
#include "services/metrics/public/cpp/delegating_ukm_recorder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "components/autofill/core/browser/ml_model/field_classification_model_handler.h"
#endif

namespace autofill {

class TestAutofillClient;

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
template <std::derived_from<AutofillClient> T>
class TestAutofillClientTemplate : public T {
 public:
  using T::T;
  TestAutofillClientTemplate(const TestAutofillClientTemplate&) = delete;
  TestAutofillClientTemplate& operator=(const TestAutofillClientTemplate&) =
      delete;
  ~TestAutofillClientTemplate() override = default;

  base::WeakPtr<AutofillClient> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  const std::string& GetAppLocale() const override { return app_locale_; }

  version_info::Channel GetChannel() const override {
    return channel_for_testing_;
  }

  bool IsOffTheRecord() const override { return is_off_the_record_; }

  AutofillCrowdsourcingManager& GetCrowdsourcingManager() override {
    if (!crowdsourcing_manager_) {
      crowdsourcing_manager_ =
          std::make_unique<testing::NiceMock<MockAutofillCrowdsourcingManager>>(
              this, GetCurrentLogManager());
    }
    return *crowdsourcing_manager_;
  }

  TestVotesUploader& GetVotesUploader() override {
    if (!votes_uploader_) {
      votes_uploader_ = std::make_unique<TestVotesUploader>(this);
    }
    return *votes_uploader_;
  }

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    return test_shared_loader_factory_;
  }

  TestPersonalDataManager& GetPersonalDataManager() override {
    if (!test_personal_data_manager_) {
      test_personal_data_manager_ = std::make_unique<TestPersonalDataManager>();
      test_personal_data_manager_->SetPrefService(GetPrefs());
    }
    return *test_personal_data_manager_.get();
  }

  ValuablesDataManager* GetValuablesDataManager() override {
    if (!valuables_data_manager_) {
      valuables_data_manager_ = std::make_unique<TestValuablesDataManager>();
    }
    return valuables_data_manager_.get();
  }

  EntityDataManager* GetEntityDataManager() override {
    return entity_data_manager_non_owning_
               ? entity_data_manager_non_owning_.get()
               : entity_data_manager_.get();
  }

  MockAutofillOptimizationGuideDecider* GetAutofillOptimizationGuideDecider()
      const override {
    return mock_autofill_optimization_guide_decider_.get();
  }

  void ResetAutofillOptimizationGuideDecider() {
    mock_autofill_optimization_guide_decider_.reset();
  }

  MockAutofillAiManager* GetAutofillAiManager() override {
    return mock_autofill_ai_delegate_.get();
  }

  SingleFieldFillRouter& GetSingleFieldFillRouter() override {
    if (!single_field_fill_router_) {
      single_field_fill_router_ = std::make_unique<SingleFieldFillRouter>(
          GetAutocompleteHistoryManager(),
          GetPaymentsAutofillClient()->GetIbanManager(),
          GetPaymentsAutofillClient()->GetMerchantPromoCodeManager());
    }
    return *single_field_fill_router_;
  }

  AutocompleteHistoryManager* GetAutocompleteHistoryManager() override {
    return &mock_autocomplete_history_manager_;
  }

  AutofillPlusAddressDelegate* GetPlusAddressDelegate() override {
    return plus_address_delegate_.get();
  }

  IdentityCredentialDelegate* GetIdentityCredentialDelegate() override {
    return identity_credential_delegate_.get();
  }

  PasswordManagerDelegate* GetPasswordManagerDelegate(
      const autofill::FieldGlobalId& field_id) override {
    return password_manager_delegate_.get();
  }

  test::AutofillTestingPrefService* GetPrefs() override {
    if (!prefs_) {
      prefs_ = autofill::test::PrefServiceForTesting();
    }
    return prefs_.get();
  }

  const test::AutofillTestingPrefService* GetPrefs() const override {
    return const_cast<TestAutofillClientTemplate*>(this)->GetPrefs();
  }

  syncer::SyncService* GetSyncService() override { return test_sync_service_; }

  signin::IdentityManager* GetIdentityManager() override {
    return const_cast<signin::IdentityManager*>(
        std::as_const(*this).GetIdentityManager());
  }

  const signin::IdentityManager* GetIdentityManager() const override {
    return identity_test_env_.identity_manager();
  }

  FormDataImporter* GetFormDataImporter() override {
    if (!form_data_importer_) {
      form_data_importer_ = std::make_unique<FormDataImporter>(
          /*client=*/this,
          /*history_service=*/nullptr);
    }
    return form_data_importer_.get();
  }

  payments::TestPaymentsAutofillClient* GetPaymentsAutofillClient() override {
    if (!payments_autofill_client_) {
      payments_autofill_client_ =
          std::make_unique<payments::TestPaymentsAutofillClient>(this);
    }
    return payments_autofill_client_.get();
  }

  TestStrikeDatabase* GetStrikeDatabase() override {
    return test_strike_database_.get();
  }

  ukm::TestAutoSetUkmRecorder* GetUkmRecorder() override {
    return &test_ukm_recorder_;
  }

  TestAddressNormalizer* GetAddressNormalizer() override {
    return &test_address_normalizer_;
  }

  FastCheckoutClient* GetFastCheckoutClient() override {
    return &mock_fast_checkout_client_;
  }

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  FieldClassificationModelHandler* GetAutofillFieldClassificationModelHandler()
      override {
    return autofill_ml_prediction_model_handler_.get();
  }

  void set_autofill_ml_prediction_model_handler(
      std::unique_ptr<FieldClassificationModelHandler> handler) {
    autofill_ml_prediction_model_handler_ = std::move(handler);
  }

  FieldClassificationModelHandler*
  GetPasswordManagerFieldClassificationModelHandler() override {
    return password_ml_prediction_model_handler_.get();
  }

  void set_password_ml_prediction_model_handler(
      std::unique_ptr<FieldClassificationModelHandler> handler) {
    password_ml_prediction_model_handler_ = std::move(handler);
  }
#endif

  const GURL& GetLastCommittedPrimaryMainFrameURL() const override {
    return last_committed_primary_main_frame_url_;
  }

  url::Origin GetLastCommittedPrimaryMainFrameOrigin() const override {
    return last_committed_primary_main_frame_origin_;
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

  void ShowAutofillSettings(SuggestionType suggestion_type) override {}

  void ConfirmSaveAddressProfile(
      const AutofillProfile& profile,
      const AutofillProfile* original_profile,
      AutofillClient::SaveAddressBubbleType save_address_bubble_type,
      AutofillClient::AddressProfileSavePromptCallback callback) override {}

  AutofillClient::SuggestionUiSessionId ShowAutofillSuggestions(
      const AutofillClient::PopupOpenArgs& open_args,
      base::WeakPtr<AutofillSuggestionDelegate> delegate) override {
    is_showing_popup_ = true;
    static AutofillClient::SuggestionUiSessionId::Generator generator;
    return generator.GenerateNextId();
  }

  void UpdateAutofillDataListValues(
      base::span<const SelectOption> options) override {}

  void SetAutofillSuggestions(std::vector<Suggestion> suggestions) {
    suggestions_ = std::move(suggestions);
  }

  base::span<const Suggestion> GetAutofillSuggestions() const override {
    return suggestions_;
  }

  void UpdateAutofillSuggestions(
      const std::vector<Suggestion>& suggestions,
      FillingProduct main_filling_product,
      AutofillSuggestionTriggerSource trigger_source) override {}

  std::optional<AutofillClient::SuggestionUiSessionId>
  GetSessionIdForCurrentAutofillSuggestions() const override {
    return suggestion_ui_session_id_;
  }

  void HideAutofillSuggestions(SuggestionHidingReason reason) override {
    popup_hidden_reason_ = reason;
    is_showing_popup_ = false;
  }

  bool IsShowingAutofillPopup() { return is_showing_popup_; }

  SuggestionHidingReason popup_hiding_reason() { return popup_hidden_reason_; }

  bool ShowAutofillFieldIphForFeature(
      const FormFieldData& field,
      AutofillClient::IphFeature feature) override {
    autofill_iph_showing_ = feature;
    return true;
  }

  void HideAutofillFieldIph() override { autofill_iph_showing_ = std::nullopt; }

  bool IsShowingAutofillAiIph() {
    return autofill_iph_showing_ == AutofillClient::IphFeature::kAutofillAi;
  }

  void NotifyIphFeatureUsed(AutofillClient::IphFeature feature) override {
    if (notify_iph_feature_used_mock_callback_) {
      notify_iph_feature_used_mock_callback_->Run(feature);
    }
  }

  bool IsAutofillEnabled() const override {
    return IsAutofillProfileEnabled() ||
           AutofillClient::GetPaymentsAutofillClient()
               ->IsAutofillPaymentMethodsEnabled();
  }

  bool IsAutofillProfileEnabled() const override {
    return autofill_profile_enabled_;
  }

  bool IsWalletStorageEnabled() const override {
    return wallet_storage_enabled_;
  }

  bool IsAutocompleteEnabled() const override { return true; }

  bool IsPasswordManagerEnabled() const override { return true; }

  void DidFillForm(AutofillTriggerSource trigger_source,
                   bool is_refill) override {}

  bool IsContextSecure() const override {
    return last_committed_primary_main_frame_url_.SchemeIs("https");
  }

  bool IsCvcSavingSupported() const override {
    return is_cvc_saving_supported_;
  }

  bool IsCreditCardUploadEnabled() const override {
    return is_credit_card_upload_enabled_;
  }

  LogManager* GetCurrentLogManager() override { return log_manager_.get(); }

  autofill_metrics::FormInteractionsUkmLogger& GetFormInteractionsUkmLogger()
      override {
    return form_interactions_ukm_logger_;
  }

  const AutofillAblationStudy& GetAblationStudy() const override {
    static const base::NoDestructor<AutofillAblationStudy>
        default_ablation_study("seed");
    return *default_ablation_study;
  }

  bool ShouldFormatForLargeKeyboardAccessory() const override {
    return format_for_large_keyboard_accessory_;
  }

  FormInteractionsFlowId GetCurrentFormInteractionsFlowId() override {
    return {};
  }

  std::unique_ptr<device_reauth::DeviceAuthenticator> GetDeviceAuthenticator()
      override {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
    if (device_authenticator_) {
      return std::move(device_authenticator_);
    }
    return std::make_unique<device_reauth::MockDeviceAuthenticator>();
#else
    return nullptr;
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
  }

  device_reauth::MockDeviceAuthenticator* GetDeviceAuthenticatorPtr() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
    return device_authenticator_.get();
#else
    return nullptr;
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
  }

  void SetDeviceAuthenticator(
      std::unique_ptr<device_reauth::MockDeviceAuthenticator>
          device_authenticator) {
    device_authenticator_ = std::move(device_authenticator);
  }

#if BUILDFLAG(IS_IOS)
  bool IsLastQueriedField(FieldGlobalId field_id) override { return true; }
#endif

  OtpPhishGuardDelegate* GetOtpPhishGuardDelegate() override {
    return otp_phish_guard_delegate_.get();
  }

  void set_otp_phish_guard_delegate(
      std::unique_ptr<OtpPhishGuardDelegate> otp_phish_guard_delegate) {
    otp_phish_guard_delegate_ = std::move(otp_phish_guard_delegate);
  }

  void set_test_addresses(
      std::vector<AutofillProfile> test_addresses) override {
    for (AutofillProfile& profile : test_addresses) {
      profile.set_is_devtools_testing_profile(true);
    }
    test_addresses_ = std::move(test_addresses);
  }

  base::span<const AutofillProfile> GetTestAddresses() const override {
    return test_addresses_;
  }

  void SetPrefs(std::unique_ptr<test::AutofillTestingPrefService> prefs) {
    prefs_ = std::move(prefs);
  }

  void SetAutofillProfileEnabled(bool autofill_profile_enabled) {
    autofill_profile_enabled_ = autofill_profile_enabled;
    if (PrefService* prefs = GetPrefs()) {
      prefs->SetBoolean(prefs::kAutofillProfileEnabled,
                        autofill_profile_enabled);
    }
    if (!autofill_profile_enabled_) {
      // Profile data is refreshed when this pref is changed.
      GetPersonalDataManager().test_address_data_manager().ClearProfiles();
    }
  }

  void SetWalletStorageEnabled(bool wallet_storage_enabled) {
    wallet_storage_enabled_ = wallet_storage_enabled;
  }

  // Sets up prefs and identity state to simulate an opted-in AutofillAI user.
  // Returns `true` iff the setup was successful.
  bool SetUpPrefsAndIdentityForAutofillAi() {
    SetAutofillProfileEnabled(true);
    GetPrefs()->registry()->RegisterIntegerPref(
        optimization_guide::prefs::
            kAutofillPredictionImprovementsEnterprisePolicyAllowed,
        base::to_underlying(optimization_guide::model_execution::prefs::
                                ModelExecutionEnterprisePolicyValue::kAllow),
        PrefRegistry::LOSSY_PREF);

    identity_test_environment().MakePrimaryAccountAvailable(
        "foo@gmail.com", signin::ConsentLevel::kSignin);
    SetCanUseModelExecutionFeatures(true);
    SetVariationConfigCountryCode(GeoIpCountryCode("US"));
    return SetAutofillAiOptInStatus(*this, AutofillAiOptInStatus::kOptedIn);
  }

  // Updates whether the currently signed in primary account can use model
  // execution features. CHECKs that there is a primary account.
  void SetCanUseModelExecutionFeatures(bool can_use_model_execution) {
    AccountInfo account_info = GetIdentityManager()->FindExtendedAccountInfo(
        GetIdentityManager()->GetPrimaryAccountInfo(
            signin::ConsentLevel::kSignin));
    CHECK(!account_info.account_id.empty());
    AccountCapabilitiesTestMutator(&account_info.capabilities)
        .set_can_use_model_execution_features(can_use_model_execution);
    signin::UpdateAccountInfoForAccount(GetIdentityManager(), account_info);
  }

  void set_entity_data_manager(
      std::unique_ptr<EntityDataManager> entity_data_manager) {
    entity_data_manager_non_owning_ = nullptr;
    entity_data_manager_ = std::move(entity_data_manager);
  }

  void set_entity_data_manager(EntityDataManager* entity_data_manager) {
    entity_data_manager_.reset();
    entity_data_manager_non_owning_ = entity_data_manager;
  }

  void set_payments_autofill_client(
      std::unique_ptr<payments::TestPaymentsAutofillClient> payments_client) {
    payments_autofill_client_ = std::move(payments_client);
  }

  void set_valuables_data_manager(
      std::unique_ptr<ValuablesDataManager> valuables_data_manager) {
    valuables_data_manager_ = std::move(valuables_data_manager);
  }

  void set_single_field_fill_router(
      std::unique_ptr<SingleFieldFillRouter> router) {
    single_field_fill_router_ = std::move(router);
  }

  void set_test_strike_database(
      std::unique_ptr<TestStrikeDatabase> test_strike_database) {
    test_strike_database_ = std::move(test_strike_database);
  }

  void set_sync_service(syncer::SyncService* test_sync_service) {
    test_sync_service_ = test_sync_service;
  }

  void set_security_level(security_state::SecurityLevel security_level) {
    security_level_ = security_level;
  }

  void set_last_committed_primary_main_frame_url(const GURL& url) {
    last_committed_primary_main_frame_url_ = url;
    last_committed_primary_main_frame_origin_ = url::Origin::Create(url);
  }

  void SetVariationConfigCountryCode(
      const GeoIpCountryCode& variation_config_country_code) {
    variation_config_country_code_ = variation_config_country_code;
    GetPersonalDataManager()
        .test_address_data_manager()
        .SetVariationCountryCode(variation_config_country_code);
  }

  void set_should_save_autofill_profiles(bool value) {
    should_save_autofill_profiles_ = value;
  }

  void set_format_for_large_keyboard_accessory(
      bool format_for_large_keyboard_accessory) {
    format_for_large_keyboard_accessory_ = format_for_large_keyboard_accessory;
  }

  void set_app_locale(std::string app_locale) {
    app_locale_ = std::move(app_locale);
  }

  void set_channel_for_testing(const version_info::Channel channel) {
    channel_for_testing_ = channel;
  }

  void set_is_off_the_record(bool is_off_the_record) {
    is_off_the_record_ = is_off_the_record;
  }

  void set_is_cvc_saving_supported(bool is_cvc_saving_supported) {
    is_cvc_saving_supported_ = is_cvc_saving_supported;
  }

  void set_is_credit_card_upload_enabled(bool is_credit_card_upload_enabled) {
    is_credit_card_upload_enabled_ = is_credit_card_upload_enabled;
  }

  void set_crowdsourcing_manager(
      std::unique_ptr<AutofillCrowdsourcingManager> crowdsourcing_manager) {
    crowdsourcing_manager_ = std::move(crowdsourcing_manager);
  }

  void set_shared_url_loader_factory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
    test_shared_loader_factory_ = url_loader_factory;
  }

  void set_plus_address_delegate(
      std::unique_ptr<AutofillPlusAddressDelegate> plus_address_delegate) {
    plus_address_delegate_ = std::move(plus_address_delegate);
  }

  void set_identity_credential_delegate(
      std::unique_ptr<IdentityCredentialDelegate>
          identity_credential_delegate) {
    identity_credential_delegate_ = std::move(identity_credential_delegate);
  }

  void set_password_manager_delegate(
      std::unique_ptr<PasswordManagerDelegate> password_manager_delegate) {
    password_manager_delegate_ = std::move(password_manager_delegate);
  }

  void set_suggestion_ui_session_id(
      std::optional<AutofillClient::SuggestionUiSessionId> session_id) {
    suggestion_ui_session_id_ = session_id;
  }

  void set_notify_iph_feature_used_mock_callback(
      base::RepeatingCallback<void(AutofillClient::IphFeature)> callback) {
    notify_iph_feature_used_mock_callback_ = std::move(callback);
  }

  signin::IdentityTestEnvironment& identity_test_environment() {
    return identity_test_env_;
  }

  // Allows to return an injected SMS OTP backend which can be set using the
  // `set_sms_otp_backend`. If no backend is injected, it'll return null.
  one_time_tokens::SmsOtpBackend* GetSmsOtpBackend() const {
    return injected_sms_otp_backend_ ? injected_sms_otp_backend_.get()
                                     : nullptr;
  }

  void set_sms_otp_backend(
      std::unique_ptr<one_time_tokens::SmsOtpBackend> sms_otp_backend) {
    injected_sms_otp_backend_ = std::move(sms_otp_backend);
  }

  one_time_tokens::OneTimeTokenService* GetOneTimeTokenService()
      const override {
    return injected_one_time_token_service_
               ? injected_one_time_token_service_.get()
               : T::GetOneTimeTokenService();
  }

  void set_one_time_token_service(
      std::unique_ptr<one_time_tokens::OneTimeTokenService>
          one_time_token_service) {
    injected_one_time_token_service_ = std::move(one_time_token_service);
  }

 private:
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
  signin::IdentityTestEnvironment identity_test_env_;
  raw_ptr<syncer::SyncService> test_sync_service_ = nullptr;
  std::unique_ptr<OtpPhishGuardDelegate> otp_phish_guard_delegate_;
  std::unique_ptr<AutofillPlusAddressDelegate> plus_address_delegate_;
  std::unique_ptr<IdentityCredentialDelegate> identity_credential_delegate_;
  std::unique_ptr<PasswordManagerDelegate> password_manager_delegate_;
  TestAddressNormalizer test_address_normalizer_;
  std::unique_ptr<::testing::NiceMock<MockAutofillOptimizationGuideDecider>>
      mock_autofill_optimization_guide_decider_ = std::make_unique<
          testing::NiceMock<MockAutofillOptimizationGuideDecider>>();
  std::unique_ptr<::testing::NiceMock<MockAutofillAiManager>>
      mock_autofill_ai_delegate_ =
          std::make_unique<testing::NiceMock<MockAutofillAiManager>>(
              this,
              /*strike_database=*/nullptr);
  ::testing::NiceMock<MockAutocompleteHistoryManager>
      mock_autocomplete_history_manager_;
  ::testing::NiceMock<MockFastCheckoutClient> mock_fast_checkout_client_;
  std::unique_ptr<device_reauth::MockDeviceAuthenticator>
      device_authenticator_ = nullptr;
  std::unique_ptr<one_time_tokens::SmsOtpBackend> injected_sms_otp_backend_;
  std::unique_ptr<one_time_tokens::OneTimeTokenService>
      injected_one_time_token_service_;

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  std::unique_ptr<FieldClassificationModelHandler>
      autofill_ml_prediction_model_handler_;
  std::unique_ptr<FieldClassificationModelHandler>
      password_ml_prediction_model_handler_;
#endif

  bool autofill_profile_enabled_ = true;
  bool wallet_storage_enabled_ = true;

  // NULL by default.
  std::unique_ptr<test::AutofillTestingPrefService> prefs_;
  std::unique_ptr<TestStrikeDatabase> test_strike_database_;

  std::unique_ptr<TestPersonalDataManager> test_personal_data_manager_;
  std::unique_ptr<ValuablesDataManager> valuables_data_manager_;
  std::unique_ptr<EntityDataManager> entity_data_manager_;
  raw_ptr<EntityDataManager> entity_data_manager_non_owning_ = nullptr;
  // The below objects must be destroyed before `TestPersonalDataManager`
  // because they keep a reference to it.
  std::unique_ptr<payments::TestPaymentsAutofillClient>
      payments_autofill_client_;
  std::unique_ptr<SingleFieldFillRouter> single_field_fill_router_;
  std::unique_ptr<FormDataImporter> form_data_importer_;

  GeoIpCountryCode variation_config_country_code_;

  security_state::SecurityLevel security_level_ =
      security_state::SecurityLevel::NONE;

  bool should_save_autofill_profiles_ = true;

  bool format_for_large_keyboard_accessory_ = false;

  std::string app_locale_ = "en-US";

  version_info::Channel channel_for_testing_ = version_info::Channel::UNKNOWN;

  bool is_off_the_record_ = false;

  bool is_showing_popup_ = false;

  bool is_cvc_saving_supported_ = true;

  bool is_credit_card_upload_enabled_ = true;

  SuggestionHidingReason popup_hidden_reason_;

  std::optional<AutofillClient::IphFeature> autofill_iph_showing_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);

  // Test addresses used to allow developers to test their forms.
  std::vector<AutofillProfile> test_addresses_;

  std::vector<Suggestion> suggestions_;

  // A mock translate driver which provides the language state.
  translate::testing::MockTranslateDriver mock_translate_driver_;

  // The last URL submitted in the primary main frame by the user. Set in the
  // constructor.
  GURL last_committed_primary_main_frame_url_{"https://example.test"};
  url::Origin last_committed_primary_main_frame_origin_ =
      url::Origin::Create(last_committed_primary_main_frame_url_);

  std::optional<AutofillClient::SuggestionUiSessionId>
      suggestion_ui_session_id_;

  std::optional<base::RepeatingCallback<void(AutofillClient::IphFeature)>>
      notify_iph_feature_used_mock_callback_;

  LogRouter log_router_;
  struct LogToTerminal {
    explicit LogToTerminal(LogRouter& log_router) {
      if (base::FeatureList::IsEnabled(
              features::debug::kAutofillLogToTerminal)) {
        log_router.LogToTerminal();
      }
    }
  } log_to_terminal_{log_router_};
  std::unique_ptr<LogManager> log_manager_ =
      LogManager::Create(&log_router_, base::NullCallback());
  autofill_metrics::FormInteractionsUkmLogger form_interactions_ukm_logger_{
      this};

  std::unique_ptr<AutofillCrowdsourcingManager> crowdsourcing_manager_;
  std::unique_ptr<TestVotesUploader> votes_uploader_;

  base::WeakPtrFactory<TestAutofillClientTemplate> weak_ptr_factory_{this};
};

// Base class of TestAutofillClient. Its sole purpose is to initialize the
// TestAutofillDriverFactory before the other members of a TestAutofillClient.
//
// We want that because that's how it works for ContentAutofillClient and
// AutofillClientIOS.
//
// We achieve this by subclassing as follows:
// 1. TestAutofillClient         derives from
// 2. TestAutofillClientTemplate derives from
// 3. TestAutofillClientBase     derives from
// 4. AutofillClient
class TestAutofillClientBase : public AutofillClient {
 public:
  TestAutofillDriverFactory& GetAutofillDriverFactory() override;

 protected:
  explicit TestAutofillClientBase(base::PassKey<TestAutofillClient>);
  ~TestAutofillClientBase() override;

 private:
  TestAutofillDriverFactory autofill_driver_factory_;
};

// A simple `AutofillClient` for tests. Consider using
// `TestContentAutofillClient` and `TestAutofillClientIOS` where possible.
//
// Consider using TestAutofillClientInjector, especially in browser tests.
//
// On destruction, it destroys all TestAutofillDrivers of its
// TestAutofillDriverFactory.
class TestAutofillClient
    : public TestAutofillClientTemplate<TestAutofillClientBase> {
 public:
  TestAutofillClient();
  ~TestAutofillClient() override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_TEST_AUTOFILL_CLIENT_H_
