// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/starter.h"

#include <map>

#include "base/base64url.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/no_destructor.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/intent_strings.h"
#include "components/autofill_assistant/browser/service/api_key_fetcher.h"
#include "components/autofill_assistant/browser/service/server_url_fetcher.h"
#include "components/autofill_assistant/browser/service/service_request_sender.h"
#include "components/autofill_assistant/browser/service/service_request_sender_impl.h"
#include "components/autofill_assistant/browser/service/service_request_sender_local_impl.h"
#include "components/autofill_assistant/browser/service/simple_url_loader_factory.h"
#include "components/autofill_assistant/browser/switches.h"
#include "components/autofill_assistant/browser/trigger_scripts/dynamic_trigger_conditions.h"
#include "components/autofill_assistant/browser/trigger_scripts/static_trigger_conditions.h"
#include "components/autofill_assistant/browser/url_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace autofill_assistant {

using StartupMode = StartupUtil::StartupMode;

namespace {

// When starting trigger scripts, depending on incoming script parameters, we
// mark users as being in either the control or the experiment group to allow
// for aggregation of UKM metrics.
const char kTriggerScriptExperimentSyntheticFieldTrialName[] =
    "AutofillAssistantLiteScriptExperiment";
const char kTriggerScriptExperimentGroup[] = "Experiment";
const char kTriggerScriptControlGroup[] = "Control";

// Creates a service request sender that serves the pre-specified response.
// Creation may fail (return null) if the parameter fails to decode.
std::unique_ptr<ServiceRequestSender> CreateBase64TriggerScriptRequestSender(
    const std::string& base64_trigger_script) {
  std::string response;
  if (!base::Base64UrlDecode(base64_trigger_script,
                             base::Base64UrlDecodePolicy::IGNORE_PADDING,
                             &response)) {
    return nullptr;
  }
  return std::make_unique<ServiceRequestSenderLocalImpl>(response);
}

// Creates a service request sender that communicates with a remote endpoint.
std::unique_ptr<ServiceRequestSender> CreateRpcTriggerScriptRequestSender(
    content::BrowserContext* browser_context,
    StarterPlatformDelegate* delegate) {
  return std::make_unique<ServiceRequestSenderImpl>(
      browser_context,
      /* access_token_fetcher = */ nullptr,
      std::make_unique<NativeURLLoaderFactory>(),
      ApiKeyFetcher().GetAPIKey(delegate->GetChannel()),
      /* auth_enabled = */ false,
      /* disable_auth_if_no_access_token = */ true);
}

// The heuristic is shared across all instances and initialized on first use. As
// such, we do not support updating the heuristic while Chrome is running.
const scoped_refptr<StarterHeuristic> GetOrCreateStarterHeuristic() {
  static const base::NoDestructor<scoped_refptr<StarterHeuristic>>
      starter_heuristic(
          [] { return base::MakeRefCounted<StarterHeuristic>(); }());
  return *starter_heuristic;
}

}  // namespace

Starter::Starter(content::WebContents* web_contents,
                 StarterPlatformDelegate* platform_delegate,
                 ukm::UkmRecorder* ukm_recorder,
                 base::WeakPtr<RuntimeManagerImpl> runtime_manager)
    : content::WebContentsObserver(web_contents),
      platform_delegate_(platform_delegate),
      ukm_recorder_(ukm_recorder),
      runtime_manager_(runtime_manager),
      starter_heuristic_(GetOrCreateStarterHeuristic()) {
  CheckSettings();
}

Starter::~Starter() = default;

void Starter::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // User-initiated navigations during non-trigger-script startups will cancel
  // the startup. This is mostly intended for navigations while the onboarding
  // is being shown.
  if (IsStartupPending() && !navigation_handle->WasServerRedirect() &&
      !trigger_script_coordinator_ &&
      navigation_handle->GetURL() !=
          StartupUtil().ChooseStartupUrlForIntent(*pending_trigger_context_)) {
    Metrics::RecordDropOut(
        waiting_for_onboarding_ ? Metrics::DropOutReason::ONBOARDING_NAVIGATION
                                : Metrics::DropOutReason::NAVIGATION,
        pending_trigger_context_->GetScriptParameters().GetIntent().value_or(
            std::string()));
    CancelPendingStartup(
        Metrics::LiteScriptFinishedState::LITE_SCRIPT_CANCELED);
  }

  if (!navigation_handle->HasCommitted() || navigation_handle->IsErrorPage() ||
      navigation_handle->WasServerRedirect()) {
    return;
  }

  MaybeStartImplicitlyForUrl(navigation_handle->GetURL());
}

void Starter::MaybeStartImplicitlyForUrl(const GURL& url) {
  if (!fetch_trigger_scripts_on_navigation_ || IsStartupPending() ||
      !url.is_valid()) {
    return;
  }

  // Run the heuristic in a separate task.
  starter_heuristic_->RunHeuristicAsync(
      url, base::BindOnce(&Starter::OnHeuristicMatch,
                          weak_ptr_factory_.GetWeakPtr(), url));
}

void Starter::OnHeuristicMatch(const GURL& url, bool result) {
  if (!result || IsStartupPending() || !fetch_trigger_scripts_on_navigation_) {
    return;
  }

  // TODO(arbesser): add new command line switches to allow adding debug script
  // parameters, like DEBUG_SOCKET_ID
  Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(std::map<std::string, std::string>{
          {"ENABLED", "true"},
          {"START_IMMEDIATELY", "false"},
          {"REQUEST_TRIGGER_SCRIPT", "true"},
          {"ORIGINAL_DEEPLINK", url.spec()},
          {"INTENT", kShoppingAssistedCheckout}}),
      TriggerContext::Options{/* experiment_ids = */ std::string(),
                              /* is_cct = */ is_custom_tab_,
                              /* onboarding_shown = */ false,
                              /* is_direct_action = */ false,
                              /* initial_url = */ std::string()}));
}

bool Starter::IsStartupPending() const {
  return pending_trigger_context_ != nullptr ||
         trigger_script_coordinator_ != nullptr;
}

void Starter::OnTabInteractabilityChanged(bool is_interactable) {
  CheckSettings();
  if (trigger_script_coordinator_) {
    trigger_script_coordinator_->OnTabInteractabilityChanged(is_interactable);
  }
}

void Starter::CheckSettings() {
  bool prev_is_custom_tab = is_custom_tab_;
  is_custom_tab_ = platform_delegate_->GetIsCustomTab();
  bool switched_from_cct_to_tab = prev_is_custom_tab && !is_custom_tab_;
  bool proactive_help_setting_enabled =
      platform_delegate_->GetProactiveHelpSettingEnabled();
  bool msbb_setting_enabled =
      platform_delegate_->GetMakeSearchesAndBrowsingBetterEnabled();
  bool feature_module_installed =
      platform_delegate_->GetFeatureModuleInstalled();
  bool prev_fetch_trigger_scripts_on_navigation =
      fetch_trigger_scripts_on_navigation_;
  fetch_trigger_scripts_on_navigation_ =
      base::FeatureList::IsEnabled(
          features::kAutofillAssistantInChromeTriggering) &&
      proactive_help_setting_enabled && msbb_setting_enabled;

  // If there is a pending startup, re-check that the settings are still
  // allowing the startup to proceed. If not, cancel the startup.
  if (IsStartupPending()) {
    StartupMode startup_mode = StartupUtil().ChooseStartupModeForIntent(
        trigger_script_coordinator_ != nullptr
            ? trigger_script_coordinator_->GetTriggerContext()
            : *pending_trigger_context_,
        {msbb_setting_enabled, proactive_help_setting_enabled,
         feature_module_installed});
    switch (startup_mode) {
      case StartupMode::START_REGULAR:
        return;
      case StartupMode::START_BASE64_TRIGGER_SCRIPT:
      case StartupMode::START_RPC_TRIGGER_SCRIPT:
        if (!switched_from_cct_to_tab) {
          return;
        }
        // Trigger scripts are not allowed to persist when transitioning from
        // CCT to regular tab.
        CancelPendingStartup(Metrics::LiteScriptFinishedState::
                                 LITE_SCRIPT_CCT_TO_TAB_NOT_SUPPORTED);
        return;
      default:
        CancelPendingStartup(Metrics::LiteScriptFinishedState::
                                 LITE_SCRIPT_DISABLED_PROACTIVE_HELP_SETTING);
        return;
    }
  } else if (!prev_fetch_trigger_scripts_on_navigation &&
             fetch_trigger_scripts_on_navigation_) {
    MaybeStartImplicitlyForUrl(web_contents()->GetLastCommittedURL());
  }
}

void Starter::Start(std::unique_ptr<TriggerContext> trigger_context) {
  DCHECK(trigger_context);
  DCHECK(!trigger_context->GetDirectAction());
  CancelPendingStartup(Metrics::LiteScriptFinishedState::LITE_SCRIPT_CANCELED);
  pending_trigger_context_ = std::move(trigger_context);

  if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kAutofillAssistantForceOnboarding) == "true") {
    platform_delegate_->SetOnboardingAccepted(false);
  }
  if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kAutofillAssistantForceFirstTimeUser) == "true") {
    platform_delegate_->SetIsFirstTimeUser(true);
  }

  StartupMode startup_mode = StartupUtil().ChooseStartupModeForIntent(
      *pending_trigger_context_,
      {platform_delegate_->GetMakeSearchesAndBrowsingBetterEnabled(),
       platform_delegate_->GetProactiveHelpSettingEnabled(),
       platform_delegate_->GetFeatureModuleInstalled()});

  // Record startup metrics for trigger scripts as soon as possible to establish
  // a baseline.
  const auto& script_parameters =
      pending_trigger_context_->GetScriptParameters();
  if (script_parameters.GetRequestsTriggerScript() ||
      script_parameters.GetBase64TriggerScriptsResponseProto()) {
    Metrics::RecordLiteScriptStarted(
        ukm_recorder_, web_contents(), startup_mode,
        platform_delegate_->GetFeatureModuleInstalled(),
        platform_delegate_->GetIsFirstTimeUser());
  }

  switch (startup_mode) {
    case StartupMode::FEATURE_DISABLED:
    case StartupMode::MANDATORY_PARAMETERS_MISSING:
    case StartupMode::SETTING_DISABLED:
    case StartupMode::NO_INITIAL_URL:
      OnStartDone(/* start_regular_script = */ false);
      return;
    case StartupMode::START_BASE64_TRIGGER_SCRIPT:
    case StartupMode::START_RPC_TRIGGER_SCRIPT:
    case StartupMode::START_REGULAR:
      MaybeInstallFeatureModule(startup_mode);
      return;
  }
}

void Starter::CancelPendingStartup(Metrics::LiteScriptFinishedState state) {
  if (!IsStartupPending()) {
    return;
  }
  platform_delegate_->HideOnboarding();
  if (waiting_for_onboarding_) {
    Metrics::RecordOnboardingResult(Metrics::OnBoarding::OB_NO_ANSWER);
    Metrics::RecordOnboardingResult(Metrics::OnBoarding::OB_SHOWN);
    waiting_for_onboarding_ = false;
  }
  OnStartDone(/* start_regular_script = */ false);
  if (trigger_script_coordinator_) {
    trigger_script_coordinator_->Stop(state);
  }
  trigger_script_coordinator_.reset();
  pending_trigger_context_.reset();
}

void Starter::MaybeInstallFeatureModule(StartupMode startup_mode) {
  if (platform_delegate_->GetFeatureModuleInstalled()) {
    OnFeatureModuleInstalled(
        startup_mode,
        Metrics::FeatureModuleInstallation::DFM_ALREADY_INSTALLED);
    return;
  }

  platform_delegate_->InstallFeatureModule(
      /* show_ui = */ startup_mode == StartupMode::START_REGULAR,
      base::BindOnce(&Starter::OnFeatureModuleInstalled,
                     weak_ptr_factory_.GetWeakPtr(), startup_mode));
}

void Starter::OnFeatureModuleInstalled(
    StartupMode startup_mode,
    Metrics::FeatureModuleInstallation result) {
  Metrics::RecordFeatureModuleInstallation(result);
  if (result != Metrics::FeatureModuleInstallation::
                    DFM_FOREGROUND_INSTALLATION_SUCCEEDED &&
      result != Metrics::FeatureModuleInstallation::DFM_ALREADY_INSTALLED) {
    Metrics::RecordDropOut(
        Metrics::DropOutReason::DFM_INSTALL_FAILED,
        pending_trigger_context_->GetScriptParameters().GetIntent().value_or(
            std::string()));
    OnStartDone(/* start_regular_script = */ false);
    return;
  }

  switch (startup_mode) {
    case StartupMode::START_REGULAR:
      MaybeShowOnboarding();
      return;
    case StartupMode::START_BASE64_TRIGGER_SCRIPT:
    case StartupMode::START_RPC_TRIGGER_SCRIPT:
      StartTriggerScript();
      return;
    default:
      DCHECK(false);
      OnStartDone(/* start_regular_script = */ false);
      return;
  }
}

void Starter::StartTriggerScript() {
  const auto& script_parameters =
      pending_trigger_context_->GetScriptParameters();
  base::FieldTrialList::CreateFieldTrial(
      kTriggerScriptExperimentSyntheticFieldTrialName,
      script_parameters.GetTriggerScriptExperiment()
          ? kTriggerScriptExperimentGroup
          : kTriggerScriptControlGroup);

  std::unique_ptr<ServiceRequestSender> service_request_sender =
      platform_delegate_->GetTriggerScriptRequestSenderToInject();
  if (!service_request_sender) {
    if (script_parameters.GetBase64TriggerScriptsResponseProto().has_value()) {
      service_request_sender = CreateBase64TriggerScriptRequestSender(
          script_parameters.GetBase64TriggerScriptsResponseProto().value());
      if (!service_request_sender) {
        Metrics::RecordLiteScriptFinished(
            ukm_recorder_, web_contents(), UNSPECIFIED_TRIGGER_UI_TYPE,
            Metrics::LiteScriptFinishedState::
                LITE_SCRIPT_BASE64_DECODING_ERROR);
        OnTriggerScriptFinished(
            Metrics::LiteScriptFinishedState::LITE_SCRIPT_BASE64_DECODING_ERROR,
            std::move(pending_trigger_context_), base::nullopt);
        return;
      }
    } else if (script_parameters.GetRequestsTriggerScript().value_or(false)) {
      service_request_sender = CreateRpcTriggerScriptRequestSender(
          web_contents()->GetBrowserContext(), platform_delegate_);
    } else {
      // Should never happen.
      DCHECK(false);
      OnStartDone(/* start_regular_script = */ false);
      return;
    }
  }
  DCHECK(service_request_sender);

  ServerUrlFetcher url_fetcher{ServerUrlFetcher::GetDefaultServerUrl()};
  GURL startup_url = StartupUtil()
                         .ChooseStartupUrlForIntent(*pending_trigger_context_)
                         .value();
  trigger_script_coordinator_ = std::make_unique<TriggerScriptCoordinator>(
      platform_delegate_, web_contents(),
      WebController::CreateForWebContents(web_contents()),
      std::move(service_request_sender),
      url_fetcher.GetTriggerScriptsEndpoint(),
      std::make_unique<StaticTriggerConditions>(
          platform_delegate_, pending_trigger_context_.get(), startup_url),
      std::make_unique<DynamicTriggerConditions>(), ukm_recorder_);

  // Note: for the duration of the trigger script, the trigger script
  // coordinator will take ownership of the pending trigger context.
  trigger_script_coordinator_->Start(
      startup_url, std::move(pending_trigger_context_),
      base::BindOnce(&Starter::OnTriggerScriptFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void Starter::OnTriggerScriptFinished(
    Metrics::LiteScriptFinishedState state,
    std::unique_ptr<TriggerContext> trigger_context,
    base::Optional<TriggerScriptProto> trigger_script) {
  // Delete the coordinator asynchronously, to give this notification time to
  // end gracefully.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&Starter::DeleteTriggerScriptCoordinator,
                                weak_ptr_factory_.GetWeakPtr()));

  if (state != Metrics::LiteScriptFinishedState::LITE_SCRIPT_PROMPT_SUCCEEDED) {
    OnStartDone(/* start_regular_script = */ false);
    return;
  }

  // Take back ownership of the trigger context.
  pending_trigger_context_ = std::move(trigger_context);

  // Note: most trigger scripts show the onboarding on their own and log a
  // different metric for the result. We need to be careful to only run the
  // regular onboarding if necessary to avoid logging metrics more than once.
  if (platform_delegate_->GetOnboardingAccepted()) {
    OnStartDone(/* start_regular_script = */ true, trigger_script);
    return;
  } else {
    MaybeShowOnboarding(trigger_script);
  }
}

void Starter::MaybeShowOnboarding(
    base::Optional<TriggerScriptProto> trigger_script) {
  if (platform_delegate_->GetOnboardingAccepted()) {
    OnOnboardingFinished(trigger_script, /* shown = */ false,
                         OnboardingResult::ACCEPTED);
    return;
  }

  // Always use bottom sheet onboarding here. Trigger scripts may show a dialog
  // onboarding, but if we have reached this part, we're already starting the
  // regular script, where we don't offer dialog onboarding.
  runtime_manager_->SetUIState(UIState::kShown);
  waiting_for_onboarding_ = true;
  platform_delegate_->ShowOnboarding(
      /* use_dialog_onboarding = */ false, *pending_trigger_context_,
      base::BindOnce(&Starter::OnOnboardingFinished,
                     weak_ptr_factory_.GetWeakPtr(), trigger_script));
}

void Starter::OnOnboardingFinished(
    base::Optional<TriggerScriptProto> trigger_script,
    bool shown,
    OnboardingResult result) {
  waiting_for_onboarding_ = false;
  auto intent =
      pending_trigger_context_->GetScriptParameters().GetIntent().value_or(
          std::string());
  switch (result) {
    case OnboardingResult::DISMISSED:
      Metrics::RecordOnboardingResult(Metrics::OnBoarding::OB_NO_ANSWER);
      Metrics::RecordDropOut(
          Metrics::DropOutReason::ONBOARDING_BACK_BUTTON_CLICKED, intent);
      break;
    case OnboardingResult::REJECTED:
      Metrics::RecordOnboardingResult(Metrics::OnBoarding::OB_CANCELLED);
      Metrics::RecordDropOut(Metrics::DropOutReason::DECLINED, intent);
      break;
    case OnboardingResult::NAVIGATION:
      Metrics::RecordOnboardingResult(Metrics::OnBoarding::OB_NO_ANSWER);
      Metrics::RecordDropOut(Metrics::DropOutReason::ONBOARDING_NAVIGATION,
                             intent);
      break;
    case OnboardingResult::ACCEPTED:
      Metrics::RecordOnboardingResult(Metrics::OnBoarding::OB_ACCEPTED);
      break;
  }
  Metrics::RecordOnboardingResult(shown ? Metrics::OnBoarding::OB_SHOWN
                                        : Metrics::OnBoarding::OB_NOT_SHOWN);

  if (result != OnboardingResult::ACCEPTED) {
    runtime_manager_->SetUIState(UIState::kNotShown);
    OnStartDone(/* start_regular_script = */ false);
    return;
  }

  // Onboarding is the last step before regular startup.
  platform_delegate_->SetOnboardingAccepted(true);
  pending_trigger_context_->SetOnboardingShown(shown);
  OnStartDone(/* start_regular_script = */ true, trigger_script);
}

void Starter::OnStartDone(bool start_regular_script,
                          base::Optional<TriggerScriptProto> trigger_script) {
  if (!start_regular_script) {
    // Catch-all to ensure that after a failed startup attempt we no longer
    // register as visible to runtime observers.
    runtime_manager_->SetUIState(UIState::kNotShown);
    pending_trigger_context_.reset();
    return;
  }

  auto startup_url =
      StartupUtil().ChooseStartupUrlForIntent(*pending_trigger_context_);
  DCHECK(startup_url.has_value());
  platform_delegate_->StartRegularScript(
      *startup_url, std::move(pending_trigger_context_), trigger_script);
}

void Starter::DeleteTriggerScriptCoordinator() {
  trigger_script_coordinator_.reset();
}

}  // namespace autofill_assistant
