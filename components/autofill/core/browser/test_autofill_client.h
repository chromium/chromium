// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_CLIENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_CLIENT_H_

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
#include "base/i18n/rtl.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_ablation_study.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_plus_address_delegate.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_manager.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/logging/log_router.h"
#include "components/autofill/core/browser/logging/text_log_receiver.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/mock_autocomplete_history_manager.h"
#include "components/autofill/core/browser/mock_autofill_optimization_guide.h"
#include "components/autofill/core/browser/mock_autofill_prediction_improvements_delegate.h"
#include "components/autofill/core/browser/mock_merchant_promo_code_manager.h"
#include "components/autofill/core/browser/payments/local_card_migration_manager.h"
#include "components/autofill/core/browser/payments/test_payments_autofill_client.h"
#include "components/autofill/core/browser/payments/test_payments_network_interface.h"
#include "components/autofill/core/browser/strike_databases/payments/test_strike_database.h"
#include "components/autofill/core/browser/test_address_normalizer.h"
#include "components/autofill/core/browser/test_form_data_importer.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/ui/mock_fast_checkout_client.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_options.h"
#include "components/autofill/core/browser/ui/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/mock_translate_driver.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/version_info/channel.h"
#include "services/metrics/public/cpp/delegating_ukm_recorder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "components/autofill/core/browser/ml_model/autofill_ml_prediction_model_handler.h"
#endif

using ::autofill::test::AutofillTestingPrefService;

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
template <std::derived_from<AutofillClient> T>
class TestAutofillClientTemplate : public T {
 public:
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

  bool IsOffTheRecord() const override { return is_off_the_record_; }

  AutofillCrowdsourcingManager* GetCrowdsourcingManager() override {
    return crowdsourcing_manager_.get();
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

  MockAutofillOptimizationGuide* GetAutofillOptimizationGuide() const override {
    return mock_autofill_optimization_guide_.get();
  }

  void ResetAutofillOptimizationGuide() {
    mock_autofill_optimization_guide_.reset();
  }

  MockAutofillPredictionImprovementsDelegate*
  GetAutofillPredictionImprovementsDelegate() override {
    return mock_autofill_prediction_improvements_delegate_.get();
  }

  AutocompleteHistoryManager* GetAutocompleteHistoryManager() override {
    return &mock_autocomplete_history_manager_;
  }

  AutofillPlusAddressDelegate* GetPlusAddressDelegate() override {
    return plus_address_delegate_.get();
  }

  AutofillTestingPrefService* GetPrefs() override {
    if (!prefs_) {
      prefs_ = autofill::test::PrefServiceForTesting();
    }
    return prefs_.get();
  }

  const AutofillTestingPrefService* GetPrefs() const override {
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
      set_test_form_data_importer(std::make_unique<FormDataImporter>(
          /*client=*/this,
          /*history_service=*/nullptr,
          /*app_locale=*/"en-US"));
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

  ukm::SourceId GetUkmSourceId() override {
    if (source_id_ == -1) {
      source_id_ = ukm::UkmRecorder::GetNewSourceID();
      test_ukm_recorder_.UpdateSourceURL(source_id_, form_origin_);
    }
    return source_id_;
  }

  TestAddressNormalizer* GetAddressNormalizer() override {
    return &test_address_normalizer_;
  }

  FastCheckoutClient* GetFastCheckoutClient() override {
    return &mock_fast_checkout_client_;
  }

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  AutofillMlPredictionModelHandler* GetAutofillMlPredictionModelHandler()
      override {
    return ml_prediction_model_handler_.get();
  }

  void set_ml_prediction_model_handler(
      std::unique_ptr<AutofillMlPredictionModelHandler> handler) {
    ml_prediction_model_handler_ = std::move(handler);
  }
#endif

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

  void ShowAutofillSettings(SuggestionType suggestion_type) override {}

  void ConfirmSaveAddressProfile(
      const AutofillProfile& profile,
      const AutofillProfile* original_profile,
      bool is_migration_to_account,
      AutofillClient::AddressProfileSavePromptCallback callback) override {}

  void ShowEditAddressProfileDialog(
      const AutofillProfile& profile,
      AutofillClient::AddressProfileSavePromptCallback
          on_user_decision_callback) override {}

  void ShowDeleteAddressProfileDialog(
      const AutofillProfile& profile,
      AutofillClient::AddressProfileDeleteDialogCallback delete_dialog_callback)
      override {}

  AutofillClient::SuggestionUiSessionId ShowAutofillSuggestions(
      const AutofillClient::PopupOpenArgs& open_args,
      base::WeakPtr<AutofillSuggestionDelegate> delegate) override {
    is_showing_popup_ = true;
    static AutofillClient::SuggestionUiSessionId::Generator generator;
    return generator.GenerateNextId();
  }

  void UpdateAutofillDataListValues(
      base::span<const SelectOption> options) override {}

  base::span<const Suggestion> GetAutofillSuggestions() const override {
    return {};
  }

  void PinAutofillSuggestions() override {}

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

  void ShowAutofillFieldIphForFeature(
      const FormFieldData& field,
      AutofillClient::IphFeature feature) override {
    autofill_iph_showing_ = feature;
  }

  void HideAutofillFieldIph() override { autofill_iph_showing_ = std::nullopt; }

  bool IsShowingManualFallbackIph() {
    return autofill_iph_showing_ == AutofillClient::IphFeature::kManualFallback;
  }

  bool IsAutocompleteEnabled() const override { return true; }

  bool IsPasswordManagerEnabled() override { return true; }

  void DidFillOrPreviewForm(mojom::ActionPersistence action_persistence,
                            AutofillTriggerSource trigger_source,
                            bool is_refill) override {}

  bool IsContextSecure() const override {
    // Simplified secure context check for tests.
    return form_origin_.SchemeIs("https");
  }

  LogManager* GetLogManager() const override { return log_manager_.get(); }

  const AutofillAblationStudy& GetAblationStudy() const override {
    static const AutofillAblationStudy default_ablation_study("seed");
    return default_ablation_study;
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

  void set_test_addresses(
      std::vector<AutofillProfile> test_addresses) override {
    test_addresses_ = std::move(test_addresses);
  }

  base::span<const AutofillProfile> GetTestAddresses() const override {
    return test_addresses_;
  }

  void SetPrefs(std::unique_ptr<AutofillTestingPrefService> prefs) {
    prefs_ = std::move(prefs);
  }

  void set_personal_data_manager(std::unique_ptr<TestPersonalDataManager> pdm) {
    // `FormDataImporter` has a reference to the PDM.
    CHECK(!form_data_importer_)
        << "Do not reset PDM after using FormDataImporter.";
    test_personal_data_manager_ = std::move(pdm);
  }

  void set_payments_autofill_client(
      std::unique_ptr<payments::TestPaymentsAutofillClient> payments_client) {
    payments_autofill_client_ = std::move(payments_client);
  }

  void set_test_strike_database(
      std::unique_ptr<TestStrikeDatabase> test_strike_database) {
    test_strike_database_ = std::move(test_strike_database);
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
    GetPersonalDataManager()
        ->test_address_data_manager()
        .SetVariationCountryCode(variation_config_country_code);
  }

  void set_should_save_autofill_profiles(bool value) {
    should_save_autofill_profiles_ = value;
  }

  void set_format_for_large_keyboard_accessory(
      bool format_for_large_keyboard_accessory) {
    format_for_large_keyboard_accessory_ = format_for_large_keyboard_accessory;
  }

  MockAutocompleteHistoryManager* GetMockAutocompleteHistoryManager() {
    return &mock_autocomplete_history_manager_;
  }

  void set_channel_for_testing(const version_info::Channel channel) {
    channel_for_testing_ = channel;
  }

  void set_is_off_the_record(bool is_off_the_record) {
    is_off_the_record_ = is_off_the_record;
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

  void set_suggestion_ui_session_id(
      std::optional<AutofillClient::SuggestionUiSessionId> session_id) {
    suggestion_ui_session_id_ = session_id;
  }

  GURL form_origin() { return form_origin_; }

  ukm::TestUkmRecorder* GetTestUkmRecorder() { return &test_ukm_recorder_; }

  signin::IdentityTestEnvironment& identity_test_environment() {
    return identity_test_env_;
  }

 private:
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
  signin::IdentityTestEnvironment identity_test_env_;
  raw_ptr<syncer::SyncService> test_sync_service_ = nullptr;
  std::unique_ptr<AutofillPlusAddressDelegate> plus_address_delegate_;
  TestAddressNormalizer test_address_normalizer_;
  std::unique_ptr<::testing::NiceMock<MockAutofillOptimizationGuide>>
      mock_autofill_optimization_guide_ =
          std::make_unique<testing::NiceMock<MockAutofillOptimizationGuide>>();
  std::unique_ptr<
      ::testing::NiceMock<MockAutofillPredictionImprovementsDelegate>>
      mock_autofill_prediction_improvements_delegate_ = std::make_unique<
          testing::NiceMock<MockAutofillPredictionImprovementsDelegate>>();
  ::testing::NiceMock<MockAutocompleteHistoryManager>
      mock_autocomplete_history_manager_;
  ::testing::NiceMock<MockFastCheckoutClient> mock_fast_checkout_client_;
  std::unique_ptr<device_reauth::MockDeviceAuthenticator>
      device_authenticator_ = nullptr;

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  std::unique_ptr<AutofillMlPredictionModelHandler>
      ml_prediction_model_handler_;
#endif

  // NULL by default.
  std::unique_ptr<AutofillTestingPrefService> prefs_;
  std::unique_ptr<TestStrikeDatabase> test_strike_database_;

  std::unique_ptr<TestPersonalDataManager> test_personal_data_manager_;
  // The below objects must be destroyed before `TestPersonalDataManager`
  // because they keep a reference to it.
  std::unique_ptr<payments::TestPaymentsAutofillClient>
      payments_autofill_client_;
  std::unique_ptr<FormDataImporter> form_data_importer_;

  GURL form_origin_{"https://example.test"};
  ukm::SourceId source_id_ = -1;
  GeoIpCountryCode variation_config_country_code_;

  security_state::SecurityLevel security_level_ =
      security_state::SecurityLevel::NONE;

  bool should_save_autofill_profiles_ = true;

  bool format_for_large_keyboard_accessory_ = false;

  version_info::Channel channel_for_testing_ = version_info::Channel::UNKNOWN;

  bool is_off_the_record_ = false;

  bool is_showing_popup_ = false;

  SuggestionHidingReason popup_hidden_reason_;

  std::optional<AutofillClient::IphFeature> autofill_iph_showing_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);

  std::unique_ptr<AutofillCrowdsourcingManager> crowdsourcing_manager_;

  // Test addresses used to allow developers to test their forms.
  std::vector<AutofillProfile> test_addresses_;

  std::vector<std::string> migration_card_selection_;

  // A mock translate driver which provides the language state.
  translate::testing::MockTranslateDriver mock_translate_driver_;

  // The last URL submitted in the primary main frame by the user. Set in the
  // constructor.
  GURL last_committed_primary_main_frame_url_{"https://example.test"};

  std::optional<AutofillClient::SuggestionUiSessionId>
      suggestion_ui_session_id_;

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

  AutofillDriverFactory& GetAutofillDriverFactory() override;

 private:
  AutofillDriverFactory autofill_driver_factory_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_CLIENT_H_
