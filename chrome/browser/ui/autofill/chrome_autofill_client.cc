// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/chrome_autofill_client.h"

#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/address_normalizer_factory.h"
#include "chrome/browser/autofill/autocomplete_history_manager_factory.h"
#include "chrome/browser/autofill/autofill_ai_model_cache_factory.h"
#include "chrome/browser/autofill/autofill_ai_model_executor_factory.h"
#include "chrome/browser/autofill/autofill_entity_data_manager_factory.h"
#include "chrome/browser/autofill/autofill_optimization_guide_factory.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/strike_database_factory.h"
#include "chrome/browser/autofill/ui/ui_util.h"
#include "chrome/browser/autofill/valuables_data_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/device_reauth/chrome_device_authenticator_factory.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/keyboard_accessory/android/manual_filling_controller.h"
#include "chrome/browser/metrics/variations/google_groups_manager_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_manager_settings_service_factory.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/autofill/address_bubbles_controller.h"
#include "chrome/browser/ui/autofill/autofill_field_promo_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller.h"
#include "chrome/browser/ui/autofill/edit_address_profile_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/chrome_payments_autofill_client.h"
#include "chrome/browser/ui/autofill/payments/credit_card_scanner_controller.h"
#include "chrome/browser/ui/autofill/payments/payments_view_factory.h"
#include "chrome/browser/ui/autofill/popup_controller_common.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/webdata_services/web_data_service_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/content/browser/content_identity_credential_delegate.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/form_import/form_data_importer.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/identity_credential/identity_credential_delegate.h"
#include "components/autofill/core/browser/integrators/optimization_guide/autofill_optimization_guide.h"
#include "components/autofill/core/browser/integrators/plus_addresses/autofill_plus_address_delegate.h"
#include "components/autofill/core/browser/logging/log_router.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/single_field_fillers/single_field_fill_router.h"
#include "components/autofill/core/browser/studies/autofill_experiments.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_otp_input_dialog_controller_impl.h"
#include "components/autofill/core/browser/ui/popup_open_enums.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/form_interactions_flow.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/compose/buildflags.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/password_form_classification_util.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_setting.h"
#include "components/password_manager/core/browser/password_manager_settings_service.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_requirements_service.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_hats_utils.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/prefs/pref_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "components/security_state/core/security_state.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/service/sync_service.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/unified_consent/pref_names.h"
#include "components/variations/service/google_groups_manager.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/geometry/rect.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/preferences/autofill/settings_navigation_helper.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/fast_checkout/fast_checkout_client_impl.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/signin/android/signin_bridge.h"
#include "chrome/browser/ui/android/autofill/autofill_accessibility_utils.h"
#include "chrome/browser/ui/android/autofill/save_update_address_profile_flow_manager.h"
#include "chrome/browser/ui/autofill/autofill_snackbar_type.h"
#include "chrome/browser/ui/autofill/payments/offer_notification_controller_android.h"
#include "components/autofill/core/browser/payments/autofill_save_card_infobar_delegate_mobile.h"
#include "components/autofill/core/browser/payments/autofill_save_card_infobar_mobile.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/messages/android/messages_feature.h"
#include "components/strings/grit/components_strings.h"
#else  // !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/autofill_ai/chrome_autofill_ai_client.h"
#include "chrome/browser/ui/autofill/autofill_ai/save_or_update_autofill_ai_data_controller.h"
#include "chrome/browser/ui/autofill/delete_address_profile_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/offer_notification_bubble_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/plus_addresses/plus_address_error_dialog.h"
#include "chrome/browser/ui/plus_addresses/plus_address_menu_model.h"  // nogncheck
#include "chrome/browser/ui/tabs/public/tab_features.h"  // nogncheck
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_views.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_delegate.h"
#include "components/autofill_ai/core/browser/autofill_ai_manager.h"  // nogncheck
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_COMPOSE)
#include "chrome/browser/compose/chrome_compose_client.h"
#include "components/compose/core/browser/compose_manager.h"
#endif

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "chrome/browser/autofill/autofill_field_classification_model_service_factory.h"
#include "chrome/browser/password_manager/password_field_classification_model_handler_factory.h"
#include "components/autofill/core/browser/ml_model/field_classification_model_handler.h"
#endif

namespace autofill {

namespace {

// Default minimum delay in milliseconds for Plus Address HaTS surveys.
static constexpr base::TimeDelta kDefaultMinDelay = base::Seconds(10);
// Default maximum delay in milliseconds for Plus Address HaTS surveys.
static constexpr base::TimeDelta kDefaultMaxDelay = base::Seconds(60);

AutoselectFirstSuggestion ShouldAutofillPopupAutoselectFirstSuggestion(
    AutofillSuggestionTriggerSource source) {
  return AutoselectFirstSuggestion(
      source == AutofillSuggestionTriggerSource::kTextFieldDidReceiveKeyDown);
}

#if !BUILDFLAG(IS_ANDROID)
const base::Feature& GetFeature(AutofillClient::IphFeature iph_feature) {
  switch (iph_feature) {
    case AutofillClient::IphFeature::kAutofillAi:
      return feature_engagement::kIPHAutofillAiOptInFeature;
  }
  NOTREACHED();
}

ui::ElementIdentifier GetElementId(AutofillClient::IphFeature iph_feature) {
  switch (iph_feature) {
    case AutofillClient::IphFeature::kAutofillAi:
      return autofill::PopupViewViews::kAutofillAiOptInIphElementId;
  }
  NOTREACHED();
}
#endif  // !BUILDFLAG(IS_ANDROID)

void LaunchPlusAddressUserPerceptionSurvey(
    content::WebContents* web_contents,
    HatsService* hats_service,
    AutofillPlusAddressDelegate* delegate,
    plus_addresses::hats::SurveyType survey_type) {
  std::string survey_trigger;
  base::TimeDelta min_delay;
  base::TimeDelta max_delay;

  const auto get_min_delay = [](const base::Feature* feature) {
    return base::Milliseconds(
        base::FeatureParam<int>(feature, plus_addresses::hats::kMinDelayMs, 0)
            .Get());
  };
  const auto get_max_delay = [](const base::Feature* feature) {
    return base::Milliseconds(
        base::FeatureParam<int>(feature, plus_addresses::hats::kMaxDelayMs, 0)
            .Get());
  };

  switch (survey_type) {
    case plus_addresses::hats::SurveyType::kAcceptedFirstTimeCreate:
      if (!base::FeatureList::IsEnabled(
              features::kPlusAddressAcceptedFirstTimeCreateSurvey)) {
        return;
      }
      survey_trigger = kHatsSurveyTriggerPlusAddressAcceptedFirstTimeCreate;
      min_delay =
          get_min_delay(&features::kPlusAddressAcceptedFirstTimeCreateSurvey);
      max_delay =
          get_max_delay(&features::kPlusAddressAcceptedFirstTimeCreateSurvey);
      break;
    case plus_addresses::hats::SurveyType::kDeclinedFirstTimeCreate:
      if (!base::FeatureList::IsEnabled(
              features::kPlusAddressDeclinedFirstTimeCreateSurvey)) {
        return;
      }
      survey_trigger = kHatsSurveyTriggerPlusAddressDeclinedFirstTimeCreate;
      min_delay =
          get_min_delay(&features::kPlusAddressDeclinedFirstTimeCreateSurvey);
      max_delay =
          get_max_delay(&features::kPlusAddressDeclinedFirstTimeCreateSurvey);
      break;
    case plus_addresses::hats::SurveyType::kCreatedMultiplePlusAddresses:
      if (!base::FeatureList::IsEnabled(
              features::kPlusAddressUserCreatedMultiplePlusAddressesSurvey)) {
        return;
      }
      survey_trigger =
          kHatsSurveyTriggerPlusAddressCreatedMultiplePlusAddresses;
      min_delay = get_min_delay(
          &features::kPlusAddressUserCreatedMultiplePlusAddressesSurvey);
      max_delay = get_max_delay(
          &features::kPlusAddressUserCreatedMultiplePlusAddressesSurvey);
      break;
    case plus_addresses::hats::SurveyType::kCreatedPlusAddressViaManualFallback:
      if (!base::FeatureList::IsEnabled(
              features::
                  kPlusAddressUserCreatedPlusAddressViaManualFallbackSurvey)) {
        return;
      }
      survey_trigger =
          kHatsSurveyTriggerPlusAddressCreatedPlusAddressViaManualFallback;
      min_delay = get_min_delay(
          &features::kPlusAddressUserCreatedPlusAddressViaManualFallbackSurvey);
      max_delay = get_max_delay(
          &features::kPlusAddressUserCreatedPlusAddressViaManualFallbackSurvey);
      break;
    case plus_addresses::hats::SurveyType::kDidChoosePlusAddressOverEmail:
      if (!base::FeatureList::IsEnabled(
              features::kPlusAddressUserDidChoosePlusAddressOverEmailSurvey)) {
        return;
      }
      survey_trigger =
          kHatsSurveyTriggerPlusAddressDidChoosePlusAddressOverEmailSurvey;
      min_delay = get_min_delay(
          &features::kPlusAddressUserDidChoosePlusAddressOverEmailSurvey);
      max_delay = get_max_delay(
          &features::kPlusAddressUserDidChoosePlusAddressOverEmailSurvey);
      break;
    case plus_addresses::hats::SurveyType::kDidChooseEmailOverPlusAddress:
      if (!base::FeatureList::IsEnabled(
              features::kPlusAddressUserDidChooseEmailOverPlusAddressSurvey)) {
        return;
      }
      survey_trigger =
          kHatsSurveyTriggerPlusAddressDidChooseEmailOverPlusAddressSurvey;
      min_delay = get_min_delay(
          &features::kPlusAddressUserDidChooseEmailOverPlusAddressSurvey);
      max_delay = get_max_delay(
          &features::kPlusAddressUserDidChooseEmailOverPlusAddressSurvey);
      break;
    case plus_addresses::hats::SurveyType::kFilledPlusAddressViaManualFallack:
      if (!base::FeatureList::IsEnabled(
              features::kPlusAddressFilledPlusAddressViaManualFallbackSurvey)) {
        return;
      }
      survey_trigger =
          kHatsSurveyTriggerPlusAddressFilledPlusAddressViaManualFallback;
      min_delay = get_min_delay(
          &features::kPlusAddressFilledPlusAddressViaManualFallbackSurvey);
      max_delay = get_max_delay(
          &features::kPlusAddressFilledPlusAddressViaManualFallbackSurvey);
      break;
  }

  // Set default delays if the delays are not configured in the finch config or
  // are configured to invalid values.
  if (min_delay >= max_delay || min_delay.is_negative()) {
    min_delay = kDefaultMinDelay;
    max_delay = kDefaultMaxDelay;
  }
  const base::TimeDelta delay = base::RandTimeDelta(min_delay, max_delay);

  hats_service->LaunchDelayedSurveyForWebContents(
      survey_trigger, web_contents, delay.InMilliseconds(),
      /*product_specific_bits_data=*/{},
      /*product_specific_string_data=*/
      delegate->GetPlusAddressHatsData(),
      /*navigation_behaviour=*/HatsService::NavigationBehaviour::ALLOW_ANY,
      /*success_callback=*/base::DoNothing(),
      /*failure_callback=*/base::DoNothing());
}

}  // namespace

// static
void ChromeAutofillClient::CreateForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  if (!FromWebContents(web_contents)) {
    web_contents->SetUserData(
        UserDataKey(),
        base::WrapUnique(new ChromeAutofillClient(web_contents)));
  }
}

ChromeAutofillClient::~ChromeAutofillClient() {
  // NOTE: It is too late to clean up the autofill popup; that cleanup process
  // requires that the WebContents instance still be valid and it is not at
  // this point (in particular, the WebContentsImpl destructor has already
  // finished running and we are now in the base class destructor).
  if (suggestion_controller_) {
    base::debug::DumpWithoutCrashing();
    // Hide the controller to avoid a memory leak.
    suggestion_controller_->Hide(SuggestionHidingReason::kTabGone);
  }
}

base::WeakPtr<AutofillClient> ChromeAutofillClient::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

const std::string& ChromeAutofillClient::GetAppLocale() const {
  return g_browser_process->GetFeatures()->application_locale_storage()->Get();
}

version_info::Channel ChromeAutofillClient::GetChannel() const {
  return chrome::GetChannel();
}

bool ChromeAutofillClient::IsOffTheRecord() const {
  auto* mutable_this = const_cast<ChromeAutofillClient*>(this);
  return mutable_this->web_contents()->GetBrowserContext()->IsOffTheRecord();
}

scoped_refptr<network::SharedURLLoaderFactory>
ChromeAutofillClient::GetURLLoaderFactory() {
  return web_contents()
      ->GetBrowserContext()
      ->GetDefaultStoragePartition()
      ->GetURLLoaderFactoryForBrowserProcess();
}

AutofillCrowdsourcingManager& ChromeAutofillClient::GetCrowdsourcingManager() {
  if (!crowdsourcing_manager_) {
    // Lazy initialization to avoid virtual function calls in the constructor.
    crowdsourcing_manager_ =
        std::make_unique<AutofillCrowdsourcingManager>(this, GetChannel());
  }
  return *crowdsourcing_manager_;
}

VotesUploader& ChromeAutofillClient::GetVotesUploader() {
  return votes_uploader_;
}

AutofillOptimizationGuide* ChromeAutofillClient::GetAutofillOptimizationGuide()
    const {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return profile->ShutdownStarted()
             ? nullptr
             : AutofillOptimizationGuideFactory::GetForProfile(profile);
}

FieldClassificationModelHandler*
ChromeAutofillClient::GetAutofillFieldClassificationModelHandler() {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  if (base::FeatureList::IsEnabled(features::kAutofillModelPredictions)) {
    return AutofillFieldClassificationModelServiceFactory::GetForBrowserContext(
        web_contents()->GetBrowserContext());
  }
#endif
  return nullptr;
}

FieldClassificationModelHandler*
ChromeAutofillClient::GetPasswordManagerFieldClassificationModelHandler() {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  if (base::FeatureList::IsEnabled(
          password_manager::features::kPasswordFormClientsideClassifier)) {
    return PasswordFieldClassificationModelHandlerFactory::GetForBrowserContext(
        web_contents()->GetBrowserContext());
  }
#endif
  return nullptr;
}

PersonalDataManager& ChromeAutofillClient::GetPersonalDataManager() {
  return CHECK_DEREF(PersonalDataManagerFactory::GetForBrowserContext(
      web_contents()->GetBrowserContext()));
}

ValuablesDataManager* ChromeAutofillClient::GetValuablesDataManager() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return ValuablesDataManagerFactory::GetForProfile(profile);
}

EntityDataManager* ChromeAutofillClient::GetEntityDataManager() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return AutofillEntityDataManagerFactory::GetForProfile(profile);
}

SingleFieldFillRouter& ChromeAutofillClient::GetSingleFieldFillRouter() {
  return single_field_fill_router_;
}

AutocompleteHistoryManager*
ChromeAutofillClient::GetAutocompleteHistoryManager() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return AutocompleteHistoryManagerFactory::GetForProfile(profile);
}

AutofillComposeDelegate* ChromeAutofillClient::GetComposeDelegate() {
#if BUILDFLAG(ENABLE_COMPOSE)
  auto* client = ChromeComposeClient::FromWebContents(web_contents());
  return client ? &client->GetManager() : nullptr;
#else
  return nullptr;
#endif
}

AutofillPlusAddressDelegate* ChromeAutofillClient::GetPlusAddressDelegate() {
  // The `PlusAddressServiceFactory` should also ensure the service is not
  // created without the feature enabled, but being defensive here to avoid
  // surprises.
  if (!base::FeatureList::IsEnabled(
          plus_addresses::features::kPlusAddressesEnabled)) {
    return nullptr;
  }
  return PlusAddressServiceFactory::GetForBrowserContext(
      web_contents()->GetBrowserContext());
}

PasswordManagerDelegate* ChromeAutofillClient::GetPasswordManagerDelegate(
    const FieldGlobalId& field_id) {
  ChromePasswordManagerClient* client =
      ChromePasswordManagerClient::FromWebContents(web_contents());
  return client ? client->GetAutofillDelegate(field_id) : nullptr;
}

void ChromeAutofillClient::GetAiPageContent(GetAiPageContentCallback callback) {
  blink::mojom::AIPageContentOptionsPtr extraction_options =
      optimization_guide::DefaultAIPageContentOptions();
  extraction_options->on_critical_path = false;
  optimization_guide::GetAIPageContent(
      web_contents(), std::move(extraction_options),
      base::BindOnce([](std::optional<optimization_guide::AIPageContentResult>
                            result)
                         -> std::optional<
                             optimization_guide::proto::AnnotatedPageContent> {
        if (!result) {
          return std::nullopt;
        }
        // For now, discard all other metadata about the request.
        return std::move(result)->proto;
      }).Then(std::move(callback)));
}

AutofillAiDelegate* ChromeAutofillClient::GetAutofillAiDelegate() {
#if !BUILDFLAG(IS_ANDROID)
  if (tabs::TabInterface* tab = tabs::TabInterface::MaybeGetFromContents(
          web_contents()->GetOutermostWebContents())) {
    ChromeAutofillAiClient* client =
        tab->GetTabFeatures()->chrome_autofill_ai_client();
    return client ? &client->GetManager() : nullptr;
  }
#endif
  return nullptr;
}

AutofillAiModelCache* ChromeAutofillClient::GetAutofillAiModelCache() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return AutofillAiModelCacheFactory::GetForProfile(profile);
}

AutofillAiModelExecutor* ChromeAutofillClient::GetAutofillAiModelExecutor() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return AutofillAiModelExecutorFactory::GetForProfile(profile);
}

IdentityCredentialDelegate*
ChromeAutofillClient::GetIdentityCredentialDelegate() {
  if (!(base::FeatureList::IsEnabled(::features::kFedCmDelegation) ||
        base::FeatureList::IsEnabled(::features::kFedCmAutofill))) {
    return nullptr;
  }

  return &identity_credential_delegate_;
}

void ChromeAutofillClient::OfferPlusAddressCreation(
    const url::Origin& main_frame_origin,
    bool is_manual_fallback,
    PlusAddressCallback callback) {
  // The controller is owned by `web_contents()` (via `WebContentsUserData`).
  plus_addresses::PlusAddressCreationController* controller =
      plus_addresses::PlusAddressCreationController::GetOrCreate(
          web_contents());
  controller->OfferCreation(main_frame_origin, is_manual_fallback,
                            std::move(callback));
}

void ChromeAutofillClient::ShowPlusAddressError(
    PlusAddressErrorDialogType error_dialog_type,
    base::OnceClosure on_accepted) {
#if !BUILDFLAG(IS_ANDROID)
  plus_addresses::ShowInlineCreationErrorDialog(
      web_contents(), error_dialog_type, std::move(on_accepted));
#endif
}

void ChromeAutofillClient::ShowPlusAddressAffiliationError(
    std::u16string affiliated_domain,
    std::u16string affiliated_plus_address,
    base::OnceClosure on_accepted) {
#if !BUILDFLAG(IS_ANDROID)
  plus_addresses::ShowInlineCreationAffiliationErrorDialog(
      web_contents(), std::move(affiliated_domain),
      std::move(affiliated_plus_address), std::move(on_accepted));
#endif
}

PrefService* ChromeAutofillClient::GetPrefs() {
  return const_cast<PrefService*>(std::as_const(*this).GetPrefs());
}

const PrefService* ChromeAutofillClient::GetPrefs() const {
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext())
      ->GetPrefs();
}

syncer::SyncService* ChromeAutofillClient::GetSyncService() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return SyncServiceFactory::GetForProfile(profile);
}

signin::IdentityManager* ChromeAutofillClient::GetIdentityManager() {
  return const_cast<signin::IdentityManager*>(
      std::as_const(*this).GetIdentityManager());
}

const signin::IdentityManager* ChromeAutofillClient::GetIdentityManager()
    const {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return IdentityManagerFactory::GetForProfile(profile->GetOriginalProfile());
}

const GoogleGroupsManager* ChromeAutofillClient::GetGoogleGroupsManager()
    const {
  // Always return the GoogleGroupsManager of the original profile to allow us
  // to do per-profile feature checks.
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return GoogleGroupsManagerFactory::GetForBrowserContext(
      profile->GetOriginalProfile());
}

FormDataImporter* ChromeAutofillClient::GetFormDataImporter() {
  if (!form_data_importer_) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    form_data_importer_ = std::make_unique<FormDataImporter>(
        this, HistoryServiceFactory::GetForProfile(
                  profile, ServiceAccessType::EXPLICIT_ACCESS));
  }
  return form_data_importer_.get();
}

payments::ChromePaymentsAutofillClient*
ChromeAutofillClient::GetPaymentsAutofillClient() {
  return &payments_autofill_client_;
}

StrikeDatabase* ChromeAutofillClient::GetStrikeDatabase() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  // No need to return a StrikeDatabase in incognito mode. It is primarily
  // used to determine whether or not to offer save of Autofill data. However,
  // we don't allow saving of Autofill data while in incognito anyway, so an
  // incognito code path should never get far enough to query StrikeDatabase.
  return StrikeDatabaseFactory::GetForProfile(profile);
}

ukm::UkmRecorder* ChromeAutofillClient::GetUkmRecorder() {
  return ukm::UkmRecorder::Get();
}

AddressNormalizer* ChromeAutofillClient::GetAddressNormalizer() {
  return AddressNormalizerFactory::GetInstance();
}

const GURL& ChromeAutofillClient::GetLastCommittedPrimaryMainFrameURL() const {
  return web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL();
}

url::Origin ChromeAutofillClient::GetLastCommittedPrimaryMainFrameOrigin()
    const {
  return web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin();
}

security_state::SecurityLevel
ChromeAutofillClient::GetSecurityLevelForUmaHistograms() {
  SecurityStateTabHelper* helper =
      ::SecurityStateTabHelper::FromWebContents(web_contents());

  // If there is no helper, it means we are not in a "web" state (for example
  // the file picker on CrOS). Return SECURITY_LEVEL_COUNT which will not be
  // logged.
  if (!helper) {
    return security_state::SecurityLevel::SECURITY_LEVEL_COUNT;
  }

  return helper->GetSecurityLevel();
}

const translate::LanguageState* ChromeAutofillClient::GetLanguageState() {
  // TODO(crbug.com/41430413): iOS vs other platforms extracts the language from
  // the top level frame vs whatever frame directly holds the form.
  auto* translate_manager =
      ChromeTranslateClient::GetManagerFromWebContents(web_contents());
  if (translate_manager) {
    return translate_manager->GetLanguageState();
  }
  return nullptr;
}

translate::TranslateDriver* ChromeAutofillClient::GetTranslateDriver() {
  // TODO(crbug.com/41430413): iOS vs other platforms extracts the language from
  // the top level frame vs whatever frame directly holds the form.
  auto* translate_client =
      ChromeTranslateClient::FromWebContents(web_contents());
  if (translate_client) {
    return translate_client->translate_driver();
  }
  return nullptr;
}

GeoIpCountryCode ChromeAutofillClient::GetVariationConfigCountryCode() const {
  variations::VariationsService* variation_service =
      g_browser_process->variations_service();
  // Retrieves the country code from variation service and converts it to upper
  // case.
  return GeoIpCountryCode(
      variation_service
          ? base::ToUpperASCII(variation_service->GetLatestCountry())
          : std::string());
}

profile_metrics::BrowserProfileType ChromeAutofillClient::GetProfileType()
    const {
  Profile* profile = GetProfile();
  // Profile can only be null in tests, therefore it is safe to always return
  // |kRegular| when it does not exist.
  return profile ? profile_metrics::GetBrowserProfileType(profile)
                 : profile_metrics::BrowserProfileType::kRegular;
}

FastCheckoutClient* ChromeAutofillClient::GetFastCheckoutClient() {
#if BUILDFLAG(IS_ANDROID)
  return fast_checkout_client_.get();
#else
  return nullptr;
#endif
}

void ChromeAutofillClient::ShowAutofillSettings(
    SuggestionType suggestion_type) {
#if BUILDFLAG(IS_ANDROID)
  switch (suggestion_type) {
    case SuggestionType::kManageAddress:
      ShowAutofillProfileSettings(web_contents());
      return;
    case SuggestionType::kManageCreditCard:
      ShowAutofillCreditCardSettings(web_contents());
      return;
    default:
      NOTREACHED();
  }
#else
  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  if (browser) {
    switch (suggestion_type) {
      case SuggestionType::kManageAddress:
        chrome::ShowSettingsSubPage(browser, chrome::kAddressesSubPage);
        return;
      case SuggestionType::kManageAutofillAi:
        chrome::ShowSettingsSubPage(browser, chrome::kAutofillAiSubPage);
        return;
      case SuggestionType::kManagePlusAddress:
        CHECK(base::FeatureList::IsEnabled(
            plus_addresses::features::kPlusAddressesEnabled));
        ShowSingletonTab(
            browser,
            GURL(plus_addresses::features::kPlusAddressManagementUrl.Get()));
        return;
      case SuggestionType::kManageCreditCard:
      case SuggestionType::kManageIban:
        chrome::ShowSettingsSubPage(browser, chrome::kPaymentsSubPage);
        return;
      case SuggestionType::kManageLoyaltyCard:
        CHECK(base::FeatureList::IsEnabled(
            features::kAutofillEnableLoyaltyCardsFilling));
        static constexpr std::string_view kValuableManagementUrl =
            "https://wallet.google.com/wallet/passes";
        ShowSingletonTab(browser, GURL(kValuableManagementUrl));
        return;
      default:
        NOTREACHED();
    }
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

void ChromeAutofillClient::ConfirmSaveAddressProfile(
    const AutofillProfile& profile,
    const AutofillProfile* original_profile,
    bool is_migration_to_account,
    AddressProfileSavePromptCallback callback) {
#if BUILDFLAG(IS_ANDROID)
  save_update_address_profile_flow_manager_->OfferSave(
      web_contents(), profile, original_profile, is_migration_to_account,
      std::move(callback));
#else
  AddressBubblesController::SetUpAndShowSaveOrUpdateAddressBubble(
      web_contents(), profile, original_profile, is_migration_to_account,
      std::move(callback));
#endif
}

AutofillClient::SuggestionUiSessionId
ChromeAutofillClient::ShowAutofillSuggestions(
    const PopupOpenArgs& open_args,
    base::WeakPtr<AutofillSuggestionDelegate> delegate) {
  // The Autofill Popup cannot open if it overlaps with another popup.
  // Therefore, the IPH is hidden before showing the Autofill Popup.
  HideAutofillFieldIph();

  // IPH hiding is asynchronous. Posting showing the Autofill Popup
  // guarantees the IPH will be hidden by the time the Autofill Popup will
  // attempt to open. This works because the tasks of hiding the IPH and showing
  // the Autofill Popup are posted on the same thread (UI thread).
  const SuggestionUiSessionId session_id =
      AutofillSuggestionController::GenerateSuggestionUiSessionId();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ChromeAutofillClient::ShowAutofillSuggestionsImpl,
                     weak_ptr_factory_.GetWeakPtr(), session_id, open_args,
                     delegate));
  return session_id;
}

void ChromeAutofillClient::ShowPlusAddressEmailOverrideNotification(
    const std::string& original_email,
    EmailOverrideUndoCallback email_override_undo_callback) {
#if BUILDFLAG(IS_ANDROID)
  GetAutofillSnackbarController()->Show(
      AutofillSnackbarType::kPlusAddressEmailOverride,
      std::move(email_override_undo_callback));
#else
  Browser* const browser = chrome::FindBrowserWithTab(web_contents());
  if (!browser) {
    return;
  }
  if (ToastController* const controller =
          browser->browser_window_features()->toast_controller()) {
    ToastParams params(ToastId::kPlusAddressOverride);
    params.menu_model = std::make_unique<plus_addresses::PlusAddressMenuModel>(
        base::UTF8ToUTF16(
            GetIdentityManager()
                ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
                .email),
        std::move(email_override_undo_callback),
        base::BindRepeating(&AutofillClient::ShowAutofillSettings, GetWeakPtr(),
                            SuggestionType::kManagePlusAddress));
    controller->MaybeShowToast(std::move(params));
  }
#endif
}

void ChromeAutofillClient::UpdateAutofillDataListValues(
    base::span<const SelectOption> options) {
  if (suggestion_controller_.get()) {
    suggestion_controller_->UpdateDataListValues(options);
  }
}

base::span<const Suggestion> ChromeAutofillClient::GetAutofillSuggestions()
    const {
  return suggestion_controller_ ? suggestion_controller_->GetSuggestions()
                                : base::span<const Suggestion>();
}

std::optional<AutofillClient::PopupScreenLocation>
ChromeAutofillClient::GetPopupScreenLocation() const {
  return suggestion_controller_
             ? suggestion_controller_->GetPopupScreenLocation()
             : std::make_optional<AutofillClient::PopupScreenLocation>();
}

std::optional<AutofillClient::SuggestionUiSessionId>
ChromeAutofillClient::GetSessionIdForCurrentAutofillSuggestions() const {
  return suggestion_controller_ ? suggestion_controller_->GetUiSessionId()
                                : std::nullopt;
}

void ChromeAutofillClient::UpdateAutofillSuggestions(
    const std::vector<Suggestion>& suggestions,
    FillingProduct main_filling_product,
    AutofillSuggestionTriggerSource trigger_source) {
  const std::optional<SuggestionUiSessionId> session_id =
      GetSessionIdForCurrentAutofillSuggestions();
  if (!session_id) {
    return;  // Update only if there is UI showing.
  }

  // When a form changes dynamically, `suggestion_controller_` may hold a
  // delegate of the wrong type, so updating the popup would call into the wrong
  // delegate. Hence, just close the existing popup (crbug.com/1113241).
  if (main_filling_product !=
      suggestion_controller_.get()->GetMainFillingProduct()) {
    suggestion_controller_->Hide(SuggestionHidingReason::kStaleData);
    return;
  }

  // Calling show will reuse the existing view automatically.
  suggestion_controller_->Show(
      *session_id, suggestions, trigger_source,
      ShouldAutofillPopupAutoselectFirstSuggestion(trigger_source));
}

void ChromeAutofillClient::HideAutofillSuggestions(
    SuggestionHidingReason reason) {
  if (suggestion_controller_.get()) {
    suggestion_controller_->Hide(reason);
  }
}

void ChromeAutofillClient::TriggerUserPerceptionOfAutofillSurvey(
    FillingProduct filling_product,
    const std::map<std::string, std::string>& field_filling_stats_data) {
#if !BUILDFLAG(IS_ANDROID)
  CHECK(filling_product == FillingProduct::kAddress ||
        filling_product == FillingProduct::kCreditCard);
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile, /*create_if_necessary=*/true);
  CHECK(hats_service);
  if (filling_product == FillingProduct::kAddress) {
    // Also add information about whether the granular filling feature is
    // available". The goal is to correlate the user's perception of autofill
    // with the feature.
    hats_service->LaunchDelayedSurveyForWebContents(
        kHatsSurveyTriggerAutofillAddressUserPerception, web_contents(),
        /*timeout_ms=*/5000, /*product_specific_bits_data=*/{},
        field_filling_stats_data);
  } else {
    hats_service->LaunchDelayedSurveyForWebContents(
        kHatsSurveyTriggerAutofillCreditCardUserPerception, web_contents(),
        /*timeout_ms=*/5000, /*product_specific_bits_data=*/
        {}, field_filling_stats_data);
  }
#endif
}

bool ChromeAutofillClient::IsAutofillEnabled() const {
  return IsAutofillProfileEnabled() || IsAutofillPaymentMethodsEnabled();
}

bool ChromeAutofillClient::IsAutofillProfileEnabled() const {
  return prefs::IsAutofillProfileEnabled(GetPrefs());
}

bool ChromeAutofillClient::IsAutofillPaymentMethodsEnabled() const {
  return prefs::IsAutofillPaymentMethodsEnabled(GetPrefs());
}

bool ChromeAutofillClient::IsAutocompleteEnabled() const {
  return prefs::IsAutocompleteEnabled(GetPrefs());
}

bool ChromeAutofillClient::IsPasswordManagerEnabled() const {
  password_manager::PasswordManagerSettingsService* settings_service =
      PasswordManagerSettingsServiceFactory::GetForProfile(GetProfile());
  return settings_service &&
         settings_service->IsSettingEnabled(
             password_manager::PasswordManagerSetting::kOfferToSavePasswords);
}

void ChromeAutofillClient::DidFillForm(AutofillTriggerSource trigger_source,
                                       bool is_refill) {
#if BUILDFLAG(IS_ANDROID)
  if (trigger_source == AutofillTriggerSource::kTouchToFillCreditCard &&
      !is_refill) {
    // TODO(crbug.com/40900538): Test that the message was announced.
    autofill::AnnounceTextForA11y(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_A11Y_ANNOUNCE_FILLED_FORM));
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

bool ChromeAutofillClient::IsContextSecure() const {
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents());
  if (!helper) {
    return false;
  }

  const auto security_level = helper->GetSecurityLevel();
  content::NavigationEntry* entry =
      web_contents()->GetController().GetVisibleEntry();

  // Only dangerous security states should prevent autofill.
  //
  // TODO(crbug.com/41307071): Once passive mixed content and legacy TLS are
  // less common, just use IsSslCertificateValid().
  return entry && entry->GetURL().SchemeIsCryptographic() &&
         security_level != security_state::DANGEROUS;
}

LogManager* ChromeAutofillClient::GetCurrentLogManager() {
  if (!log_manager_ && log_router_ && log_router_->HasReceivers()) {
    // TODO(crbug.com/40612524): Replace the closure with a callback to
    // the renderer that indicates if log messages should be sent from the
    // renderer.
    log_manager_ = LogManager::Create(log_router_, base::NullCallback());
  }
  return log_manager_.get();
}

autofill_metrics::FormInteractionsUkmLogger&
ChromeAutofillClient::GetFormInteractionsUkmLogger() {
  return form_interactions_ukm_logger_;
}

const AutofillAblationStudy& ChromeAutofillClient::GetAblationStudy() const {
  return ablation_study_;
}

#if BUILDFLAG(IS_ANDROID)
AutofillSnackbarControllerImpl*
ChromeAutofillClient::GetAutofillSnackbarController() {
  if (!autofill_snackbar_controller_impl_) {
    autofill_snackbar_controller_impl_ =
        std::make_unique<AutofillSnackbarControllerImpl>(web_contents());
  }

  return autofill_snackbar_controller_impl_.get();
}
#endif

FormInteractionsFlowId
ChromeAutofillClient::GetCurrentFormInteractionsFlowId() {
  constexpr base::TimeDelta max_flow_time = base::Minutes(20);
  base::Time now = AutofillClock::Now();

  if (now - flow_id_date_ > max_flow_time || now < flow_id_date_) {
    flow_id_ = FormInteractionsFlowId();
    flow_id_date_ = now;
  }
  return flow_id_;
}

std::unique_ptr<device_reauth::DeviceAuthenticator>
ChromeAutofillClient::GetDeviceAuthenticator() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  device_reauth::DeviceAuthParams params(
      base::Seconds(60), device_reauth::DeviceAuthSource::kAutofill);

  return ChromeDeviceAuthenticatorFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()),
      web_contents()->GetTopLevelNativeWindow(), params);
#else
  return nullptr;
#endif
}

bool ChromeAutofillClient::ShowAutofillFieldIphForFeature(
    const FormFieldData& field,
    IphFeature autofill_feature) {
#if !BUILDFLAG(IS_ANDROID)
  if (autofill_field_promo_controller_ &&
      autofill_field_promo_controller_->IsMaybeShowing()) {
    return true;
  }

  const base::Feature& feature = GetFeature(autofill_feature);

  // [Re]create the controller if `autofill_feature` isn't the current one.
  if (!autofill_field_promo_controller_ ||
      autofill_field_promo_controller_->GetFeaturePromo().name !=
          feature.name) {
    autofill_field_promo_controller_ =
        std::make_unique<AutofillFieldPromoControllerImpl>(
            web_contents(), feature, GetElementId(autofill_feature));
  }

  autofill_field_promo_controller_->Show(field.bounds());
  return autofill_field_promo_controller_->IsMaybeShowing();
#else
  return false;
#endif  // !BUILDFLAG(IS_ANDROID)
}

void ChromeAutofillClient::HideAutofillFieldIph() {
  if (autofill_field_promo_controller_) {
    autofill_field_promo_controller_->Hide();
  }
}

void ChromeAutofillClient::NotifyIphFeatureUsed(
    AutofillClient::IphFeature feature) {
#if !BUILDFLAG(IS_ANDROID)
  // Based on the feature config, the IPH will not be shown ever again once the
  // user has used the `feature`. If the user is aware of it, then they
  // shouldn't be spammed with IPHs. The IPH code cannot know if the feature was
  // used or not unless explicitly notified.
  if (auto* interface =
          BrowserUserEducationInterface::MaybeGetForWebContentsInTab(
              web_contents())) {
    interface->NotifyFeaturePromoFeatureUsed(
        GetFeature(feature),
        FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

ChromeAutofillClient::ChromeAutofillClient(content::WebContents* web_contents)
    : ContentAutofillClient(web_contents),
      content::WebContentsObserver(web_contents),
      ablation_study_(g_browser_process->local_state()),
      identity_credential_delegate_(web_contents) {
  // Initialize StrikeDatabase so its cache will be loaded and ready to use
  // when requested by other Autofill classes.
  GetStrikeDatabase();

#if BUILDFLAG(IS_ANDROID)
  save_update_address_profile_flow_manager_ =
      std::make_unique<SaveUpdateAddressProfileFlowManager>();
  fast_checkout_client_ = std::make_unique<FastCheckoutClientImpl>(this);
#endif
}

Profile* ChromeAutofillClient::GetProfile() const {
  if (!web_contents()) {
    return nullptr;
  }
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

void ChromeAutofillClient::ShowAutofillSuggestionsImpl(
    SuggestionUiSessionId session_id,
    const PopupOpenArgs& open_args,
    base::WeakPtr<AutofillSuggestionDelegate> delegate) {
  // Convert element_bounds to be in screen space.
  const gfx::Rect client_area = web_contents()->GetContainerBounds();
  const gfx::RectF element_bounds_in_screen_space =
      open_args.element_bounds + client_area.OffsetFromOrigin();

  // Deletes or reuses the old `suggestion_controller_`.
  suggestion_controller_ = AutofillSuggestionController::GetOrCreate(
      suggestion_controller_, delegate, web_contents(),
      PopupControllerCommon(
          element_bounds_in_screen_space, open_args.text_direction,
          web_contents()->GetNativeView(), open_args.anchor_type),
      open_args.form_control_ax_id);

  suggestion_controller_->Show(
      session_id, open_args.suggestions, open_args.trigger_source,
      ShouldAutofillPopupAutoselectFirstSuggestion(open_args.trigger_source));

  // When testing, try to keep popup open when the reason to hide is one of:
  // - An external browser frame resize that is extraneous to our testing goals.
  // - Too many fields get focus one after another (for example, multiple
  // password fields being autofilled by default on Desktop).
  if (suggestion_controller_) {
    suggestion_controller_->SetKeepPopupOpenForTesting(
        keep_popup_open_for_testing_);
  }
}

std::unique_ptr<AutofillManager> ChromeAutofillClient::CreateManager(
    base::PassKey<ContentAutofillDriver> pass_key,
    ContentAutofillDriver& driver) {
  return std::make_unique<BrowserAutofillManager>(&driver);
}

void ChromeAutofillClient::set_test_addresses(
    std::vector<AutofillProfile> test_addresses) {
  test_addresses_ = std::move(test_addresses);
}

base::span<const AutofillProfile> ChromeAutofillClient::GetTestAddresses()
    const {
  return test_addresses_;
}

PasswordFormClassification ChromeAutofillClient::ClassifyAsPasswordForm(
    AutofillManager& manager,
    FormGlobalId form_id,
    FieldGlobalId field_id) const {
  return password_manager::ClassifyAsPasswordForm(manager, form_id, field_id);
}

void ChromeAutofillClient::TriggerPlusAddressUserPerceptionSurvey(
    plus_addresses::hats::SurveyType survey_type) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  auto* delegate = GetPlusAddressDelegate();
  CHECK(delegate);
  LaunchPlusAddressUserPerceptionSurvey(
      web_contents(),
      HatsServiceFactory::GetForProfile(profile,
                                        /*create_if_necessary=*/true),
      delegate, survey_type);
}

}  // namespace autofill
