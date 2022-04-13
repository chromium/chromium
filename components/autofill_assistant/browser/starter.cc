// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/starter.h"

#include "base/base64url.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "components/autofill_assistant/browser/assistant_field_trial_util.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/intent_strings.h"
#include "components/autofill_assistant/browser/service/api_key_fetcher.h"
#include "components/autofill_assistant/browser/service/cup_impl.h"
#include "components/autofill_assistant/browser/service/server_url_fetcher.h"
#include "components/autofill_assistant/browser/service/service_request_sender.h"
#include "components/autofill_assistant/browser/service/service_request_sender_impl.h"
#include "components/autofill_assistant/browser/service/service_request_sender_local_impl.h"
#include "components/autofill_assistant/browser/service/simple_url_loader_factory.h"
#include "components/autofill_assistant/browser/switches.h"
#include "components/autofill_assistant/browser/trigger_scripts/dynamic_trigger_conditions.h"
#include "components/autofill_assistant/browser/trigger_scripts/static_trigger_conditions.h"
#include "components/autofill_assistant/browser/url_utils.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace autofill_assistant {

namespace {

// When starting trigger scripts, depending on incoming script parameters, we
// mark users as being in either the control or the experiment group to allow
// for aggregation of UKM metrics.
const char kTriggerScriptExperimentSyntheticFieldTrialName[] =
    "AutofillAssistantTriggerScriptExperiment";
const char kTriggerScriptExperimentGroup[] = "Experiment";
const char kTriggerScriptControlGroup[] = "Control";

// The maximum number of items to be kept in the cache. If this number is
// exceeded, the entry that hasn't been accessed the longest is automatically
// removed.
constexpr size_t kMaxFailedTriggerScriptsCacheSize = 100;
constexpr size_t kMaxUserDenylistedCacheSize = 100;

// The duration for which cache entries are considered fresh. Stale entries in
// the cache are ignored.
constexpr base::TimeDelta kMaxFailedTriggerScriptsCacheDuration =
    base::Hours(1);
constexpr base::TimeDelta kMaxUserDenylistedCacheDuration = base::Hours(1);

// Synthetic field trial names and group names should match those specified
// in google3/analysis/uma/dashboards/
// .../variations/generate_server_hashes.py and
// .../website/components/variations_dash/variations_histogram_entry.js.
const char kTriggeredSyntheticTrial[] = "AutofillAssistantTriggered";
const char kEnabledGroupName[] = "Enabled";
const char kExperimentsSyntheticTrial[] = "AutofillAssistantExperimentsTrial";

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
    base::WeakPtr<StarterPlatformDelegate> delegate) {
  return std::make_unique<ServiceRequestSenderImpl>(
      browser_context,
      /* access_token_fetcher = */ nullptr,
      std::make_unique<cup::CUPImplFactory>(),
      std::make_unique<NativeURLLoaderFactory>(),
      ApiKeyFetcher().GetAPIKey(delegate->GetChannel()));
}

// Returns whether |trigger_context| contains either the REQUEST_TRIGGER_SCRIPT
// or the TRIGGER_SCRIPTS_BASE64 script parameter.
bool IsTriggerScriptContext(const TriggerContext& trigger_context) {
  const auto& script_parameters = trigger_context.GetScriptParameters();
  return script_parameters.GetRequestsTriggerScript() ||
         script_parameters.GetBase64TriggerScriptsResponseProto();
}

// The heuristic is shared across all instances and initialized on first use. As
// such, we do not support updating the heuristic while Chrome is running.
const scoped_refptr<StarterHeuristic> GetOrCreateStarterHeuristic() {
  static const base::NoDestructor<scoped_refptr<StarterHeuristic>>
      starter_heuristic(
          [] { return base::MakeRefCounted<StarterHeuristic>(); }());
  return *starter_heuristic;
}

// The cache of failed trigger script fetches is shared across all instances and
// initialized on first use.
base::HashingLRUCache<std::string, base::TimeTicks>*
GetOrCreateFailedTriggerScriptFetchesCache() {
  static base::NoDestructor<base::HashingLRUCache<std::string, base::TimeTicks>>
      cached_failed_trigger_script_fetches(kMaxFailedTriggerScriptsCacheSize);
  return cached_failed_trigger_script_fetches.get();
}

// Goes through the |cache| and removes entries that have gone stale, i.e.,
// entries that were added before |cutoff_ticks|.
void ClearStaleCacheEntries(
    base::HashingLRUCache<std::string, base::TimeTicks>* cache,
    base::TimeTicks cutoff_ticks) {
  // Go in reverse order until the oldest entry is younger than |cutoff_ticks|.
  for (auto it = cache->rbegin(); it != cache->rend();) {
    if (it->second > cutoff_ticks) {
      return;
    }
    it = cache->Erase(it);
  }
}

// Returns true if |cache| has an entry for |url| that is younger than
// |cutoff_ticks|, false otherwise. Does not change the order of the cache.
bool HasFreshCacheEntry(
    const base::HashingLRUCache<std::string, base::TimeTicks>& cache,
    const GURL& url,
    base::TimeTicks cutoff_ticks) {
  std::string domain = url_utils::GetOrganizationIdentifyingDomain(url);
  auto it = cache.Peek(domain);
  return (it != cache.end() && (it->second > cutoff_ticks));
}

// Returns the debug parameters for implicit triggering specified in the command
// line, or the default proto if the command line switch was not specified or
// invalid.
ImplicitTriggeringDebugParametersProto
GetImplicitTriggeringDebugParametersFromCommandLine() {
  std::string parameters =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kAutofillAssistantImplicitTriggeringDebugParameters);
  if (parameters.empty()) {
    return {};
  }

  if (!base::Base64UrlDecode(parameters,
                             base::Base64UrlDecodePolicy::IGNORE_PADDING,
                             &parameters)) {
    VLOG(1) << "Failed to base64-decode debug trigger parameters: "
            << parameters;
    return {};
  }

  ImplicitTriggeringDebugParametersProto proto;
  if (!proto.ParseFromString(parameters)) {
    VLOG(1) << "Failed to parse debug trigger parameters: " << parameters;
    return {};
  }
  return proto;
}

}  // namespace

Starter::Starter(content::WebContents* web_contents,
                 base::WeakPtr<StarterPlatformDelegate> platform_delegate,
                 ukm::UkmRecorder* ukm_recorder,
                 base::WeakPtr<RuntimeManager> runtime_manager,
                 const base::TickClock* tick_clock)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<Starter>(*web_contents),
      current_ukm_source_id_(
          ukm::GetSourceIdForWebContentsDocument(web_contents)),
      cached_failed_trigger_script_fetches_(
          GetOrCreateFailedTriggerScriptFetchesCache()),
      user_denylisted_domains_(kMaxUserDenylistedCacheSize),
      implicit_triggering_debug_parameters_(
          GetImplicitTriggeringDebugParametersFromCommandLine()),
      platform_delegate_(platform_delegate),
      ukm_recorder_(ukm_recorder),
      runtime_manager_(runtime_manager),
      starter_heuristic_(GetOrCreateStarterHeuristic()),
      tick_clock_(tick_clock) {}

Starter::~Starter() = default;

void Starter::PrimaryPageChanged(content::Page& page) {
  // Navigating away from the deeplink domain during startup OR ending up on an
  // error page will break the flow, unless a trigger script is currently
  // running (in which case, the trigger script will handle this event).
  content::RenderFrameHost& rfh = page.GetMainDocument();
  const GURL& gurl = rfh.GetLastCommittedURL();
  if (IsStartupPending() && !trigger_script_coordinator_) {
    const GURL& url_for_intent =
        StartupUtil()
            .ChooseStartupUrlForIntent(*GetPendingTriggerContext())
            .value_or(GURL());
    bool navigated_to_target_domain =
        url_utils::IsSamePublicSuffixDomain(url_for_intent, gurl) &&
        url_utils::IsAllowedSchemaTransition(url_for_intent, gurl);
    if (navigated_to_target_domain) {
      current_ukm_source_id_ = page.GetMainDocument().GetPageUkmSourceId();
      if (waiting_for_deeplink_navigation_) {
        Start(std::move(pending_trigger_context_));
      }
      // Ignore; navigations to the target domain during startup are allowed.
      return;
    }

    if (waiting_for_deeplink_navigation_) {
      if (navigated_to_target_domain) {
        Start(std::move(pending_trigger_context_));
        return;
      }
      // Note: this will record for the current domain, not the target domain.
      // There seems to be no way to avoid this.
      Metrics::RecordTriggerScriptStarted(
          ukm_recorder_, page.GetMainDocument().GetPageUkmSourceId(),
          rfh.IsErrorDocument()
              ? Metrics::TriggerScriptStarted::NAVIGATION_ERROR
              : Metrics::TriggerScriptStarted::NAVIGATED_AWAY);
      CancelPendingStartup(absl::nullopt);
    } else {
      // Regular startup was interrupted (most likely during the onboarding).
      Metrics::RecordDropOut(waiting_for_onboarding_
                                 ? Metrics::DropOutReason::ONBOARDING_NAVIGATION
                                 : Metrics::DropOutReason::NAVIGATION,
                             GetPendingTriggerContext()
                                 ->GetScriptParameters()
                                 .GetIntent()
                                 .value_or(std::string()));
      CancelPendingStartup(absl::nullopt);
    }
    // Note: do not early-return here. While the previous startup has failed, we
    // may have navigated to a new supported domain and may need to start
    // implicitly.
  }

  if (!rfh.IsErrorDocument()) {
    current_ukm_source_id_ = page.GetMainDocument().GetPageUkmSourceId();
    MaybeStartImplicitlyForUrl(gurl, current_ukm_source_id_);
  }
}

void Starter::MaybeStartImplicitlyForUrl(const GURL& url,
                                         const ukm::SourceId source_id) {
  if (!fetch_trigger_scripts_on_navigation_ || IsStartupPending() ||
      platform_delegate_->IsRegularScriptRunning() || !url.is_valid()) {
    return;
  }

  // If we have failed to fetch a trigger script for this domain before, or if
  // the user has denylisted the domain, don't try again.
  base::TimeTicks now_ticks = tick_clock_->NowTicks();
  if (HasFreshCacheEntry(*cached_failed_trigger_script_fetches_, url,
                         now_ticks - kMaxFailedTriggerScriptsCacheDuration)) {
    Metrics::RecordInChromeTriggerAction(
        ukm_recorder_, source_id,
        Metrics::InChromeTriggerAction::CACHE_HIT_UNSUPPORTED_DOMAIN);
    return;
  }
  if (HasFreshCacheEntry(user_denylisted_domains_, url,
                         now_ticks - kMaxUserDenylistedCacheDuration)) {
    Metrics::RecordInChromeTriggerAction(
        ukm_recorder_, source_id,
        Metrics::InChromeTriggerAction::USER_DENYLISTED_DOMAIN);
    return;
  }

  // Run the heuristic in a separate task.
  starter_heuristic_->RunHeuristicAsync(
      url, base::BindOnce(&Starter::OnHeuristicMatch,
                          weak_ptr_factory_.GetWeakPtr(), url, source_id));
}

void Starter::OnHeuristicMatch(const GURL& url,
                               const ukm::SourceId source_id,
                               const base::flat_set<std::string>& intents) {
  if (intents.empty()) {
    Metrics::RecordInChromeTriggerAction(
        ukm_recorder_, source_id,
        Metrics::InChromeTriggerAction::NO_HEURISTIC_MATCH);
    return;
  }
  if (IsStartupPending() || !fetch_trigger_scripts_on_navigation_) {
    Metrics::RecordInChromeTriggerAction(ukm_recorder_, source_id,
                                         Metrics::InChromeTriggerAction::OTHER);
    return;
  }

  Metrics::RecordInChromeTriggerAction(
      ukm_recorder_, source_id,
      Metrics::InChromeTriggerAction::TRIGGER_SCRIPT_REQUESTED);
  base::flat_map<std::string, std::string> script_parameters = {
      {"ENABLED", "true"},
      {"INTENT",
       base::JoinString(
           std::vector<std::string>(intents.begin(), intents.end()), ",")},
      {"START_IMMEDIATELY", "false"},
      {"REQUEST_TRIGGER_SCRIPT", "true"},
      {"ORIGINAL_DEEPLINK", url.spec()},
      {"CALLER", "7"}};
  // Add/overwrite with debug parameters if specified.
  for (const auto& debug_param :
       implicit_triggering_debug_parameters_.additional_script_parameters()) {
    script_parameters[debug_param.name()] = debug_param.value();
  }

  Start(std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(script_parameters),
      TriggerContext::Options{/* experiment_ids = */ std::string(),
                              /* is_cct = */ is_custom_tab_,
                              /* onboarding_shown = */ false,
                              /* is_direct_action = */ false,
                              /* initial_url = */ std::string(),
                              /* is_in_chrome_triggered = */ true}));
}

bool Starter::IsStartupPending() const {
  return GetPendingTriggerContext() != nullptr;
}

TriggerContext* Starter::GetPendingTriggerContext() const {
  if (trigger_script_coordinator_) {
    return &trigger_script_coordinator_->GetTriggerContext();
  }
  return pending_trigger_context_.get();
}

void Starter::RegisterSyntheticFieldTrials(
    const TriggerContext& trigger_context) const {
  std::unique_ptr<AssistantFieldTrialUtil> field_trial_util =
      platform_delegate_->CreateFieldTrialUtil();
  if (!field_trial_util) {
    // Failsafe, should never happen.
    NOTREACHED();
    return;
  }

  field_trial_util->RegisterSyntheticFieldTrial(kTriggeredSyntheticTrial,
                                                kEnabledGroupName);
  // Synthetic trial for experiments.
  for (const std::string& experiment_id :
       trigger_context.GetScriptParameters().GetExperiments()) {
    field_trial_util->RegisterSyntheticFieldTrial(kExperimentsSyntheticTrial,
                                                  experiment_id);
  }
}

void Starter::OnTabInteractabilityChanged(bool is_interactable) {
  Init();
  if (trigger_script_coordinator_) {
    trigger_script_coordinator_->OnTabInteractabilityChanged(is_interactable);
  }
}

void Starter::Init() {
  if (!platform_delegate_) {
    return;
  }
  if (!platform_delegate_->IsAttached()) {
    CancelPendingStartup(Metrics::TriggerScriptFinishedState::CANCELED);
    return;
  }

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
  // Note: the feature flag must be the last thing tested in this if-statement,
  // to avoid tagging tabs that otherwise don't qualify for in-cct triggering,
  // which leads to pollution of our metrics.
  fetch_trigger_scripts_on_navigation_ =
      proactive_help_setting_enabled && msbb_setting_enabled &&
      (!platform_delegate_->GetIsWebLayer() ||
       platform_delegate_->GetIsLoggedIn()) &&
      ((is_custom_tab_ && platform_delegate_->GetIsTabCreatedByGSA() &&
        base::FeatureList::IsEnabled(
            features::kAutofillAssistantInCCTTriggering)) ||
       (!is_custom_tab_ && base::FeatureList::IsEnabled(
                               features::kAutofillAssistantInTabTriggering)));

  // If there is a pending startup, re-check that the settings are still
  // allowing the startup to proceed. If not, cancel the startup.
  if (IsStartupPending()) {
    StartupMode startup_mode = StartupUtil().ChooseStartupModeForIntent(
        *GetPendingTriggerContext(),
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
        CancelPendingStartup(
            Metrics::TriggerScriptFinishedState::CCT_TO_TAB_NOT_SUPPORTED);
        return;
      default:
        CancelPendingStartup(Metrics::TriggerScriptFinishedState::
                                 DISABLED_PROACTIVE_HELP_SETTING);
        return;
    }
  } else if (!prev_fetch_trigger_scripts_on_navigation &&
             fetch_trigger_scripts_on_navigation_) {
    MaybeStartImplicitlyForUrl(
        web_contents()->GetLastCommittedURL(),
        ukm::GetSourceIdForWebContentsDocument(web_contents()));
  }
}

void Starter::RecordDependenciesInvalidated() const {
  Metrics::DependenciesInvalidated dependencies_invalidated =
      Metrics::DependenciesInvalidated::OUTSIDE_FLOW;
  if (platform_delegate_->IsRegularScriptRunning()) {
    dependencies_invalidated = Metrics::DependenciesInvalidated::DURING_FLOW;
  } else if (IsStartupPending()) {
    dependencies_invalidated = Metrics::DependenciesInvalidated::DURING_STARTUP;
  }

  Metrics::RecordDependenciesInvalidated(dependencies_invalidated);
}

void Starter::OnDependenciesInvalidated() {
  RecordDependenciesInvalidated();
  Init();
}

void Starter::CanStart(
    std::unique_ptr<TriggerContext> trigger_context,
    base::OnceCallback<void(bool success,
                            absl::optional<GURL> url,
                            std::unique_ptr<TriggerContext> trigger_contexts)>
        preconditions_checked_callback) {
  preconditions_checked_callback_ = std::move(preconditions_checked_callback);
  Start(std::move(trigger_context));
}

void Starter::Start(std::unique_ptr<TriggerContext> trigger_context) {
  DCHECK(trigger_context);
  DCHECK(!trigger_context->GetDirectAction());

  if (!platform_delegate_) {
    return;
  }

  // Register synthetic trial as soon as possible.
  RegisterSyntheticFieldTrials(*trigger_context);

  CancelPendingStartup(Metrics::TriggerScriptFinishedState::CANCELED);
  pending_trigger_context_ = std::move(trigger_context);
  if (!platform_delegate_->IsAttached()) {
    OnStartDone(/* start_regular_script = */ false);
  }

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
  Metrics::RecordStartRequest(ukm_recorder_, current_ukm_source_id_,
                              pending_trigger_context_->GetScriptParameters(),
                              startup_mode);
  // Trigger scripts may need to wait for navigation to the deeplink domain to
  // ensure that UKMs are recorded for the right source-id.
  auto startup_url =
      StartupUtil().ChooseStartupUrlForIntent(*pending_trigger_context_);
  if (IsTriggerScriptContext(*pending_trigger_context_) &&
      !startup_url.has_value()) {
    // Fail immediately if there is no deeplink domain to wait for.
    // Note: this will record the impression for the current domain.
    Metrics::RecordTriggerScriptStarted(
        ukm_recorder_, current_ukm_source_id_,
        Metrics::TriggerScriptStarted::NO_INITIAL_URL);
    OnStartDone(/* start_script = */ false);
    return;
  }

  if (IsTriggerScriptContext(*pending_trigger_context_) &&
      !url_utils::IsSamePublicSuffixDomain(
          web_contents()->GetMainFrame()->GetLastCommittedURL(),
          startup_url.value_or(GURL()))) {
    waiting_for_deeplink_navigation_ = true;
    return;
  }

  // Record startup metrics for trigger scripts as soon as possible to establish
  // a baseline.
  if (IsTriggerScriptContext(*pending_trigger_context_)) {
    Metrics::RecordTriggerScriptStarted(
        ukm_recorder_, current_ukm_source_id_, startup_mode,
        platform_delegate_->GetFeatureModuleInstalled(),
        platform_delegate_->GetIsFirstTimeUser());
  }

  switch (startup_mode) {
    case StartupMode::FEATURE_DISABLED:
    case StartupMode::MANDATORY_PARAMETERS_MISSING:
    case StartupMode::SETTING_DISABLED:
    case StartupMode::NO_INITIAL_URL:
      OnStartDone(/* start_script = */ false);
      return;
    case StartupMode::START_BASE64_TRIGGER_SCRIPT:
    case StartupMode::START_RPC_TRIGGER_SCRIPT:
    case StartupMode::START_REGULAR:
      MaybeInstallFeatureModule(startup_mode);
      return;
  }
}

void Starter::CancelPendingStartup(
    absl::optional<Metrics::TriggerScriptFinishedState> state) {
  if (!IsStartupPending()) {
    return;
  }
  platform_delegate_->HideOnboarding();
  if (waiting_for_onboarding_) {
    Metrics::RecordRegularScriptOnboarding(ukm_recorder_,
                                           current_ukm_source_id_,
                                           Metrics::Onboarding::OB_NO_ANSWER);
    Metrics::RecordRegularScriptOnboarding(
        ukm_recorder_, current_ukm_source_id_, Metrics::Onboarding::OB_SHOWN);
    waiting_for_onboarding_ = false;
  }
  OnStartDone(/* start_script = */ false);
  if (trigger_script_coordinator_ && state) {
    trigger_script_coordinator_->Stop(*state);
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
    OnStartDone(/* start_script = */ false);
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
      OnStartDone(/* start_script = */ false);
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
        Metrics::RecordTriggerScriptFinished(
            ukm_recorder_, current_ukm_source_id_,
            TriggerScriptProto::UNSPECIFIED_TRIGGER_UI_TYPE,
            Metrics::TriggerScriptFinishedState::BASE64_DECODING_ERROR);
        OnTriggerScriptFinished(
            Metrics::TriggerScriptFinishedState::BASE64_DECODING_ERROR,
            std::move(pending_trigger_context_), absl::nullopt);
        return;
      }
    } else if (script_parameters.GetRequestsTriggerScript().value_or(false)) {
      service_request_sender = CreateRpcTriggerScriptRequestSender(
          web_contents()->GetBrowserContext(), platform_delegate_);
    } else {
      // Should never happen.
      DCHECK(false);
      OnStartDone(/* start_script = */ false);
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
      WebController::CreateForWebContents(
          web_contents(),
          /* user_data= */ nullptr,
          /* log_info= */ nullptr,
          /* annotate_dom_model_service= */ nullptr,
          /* enable_full_stack_traces= */ false),
      std::move(service_request_sender),
      url_fetcher.GetTriggerScriptsEndpoint(),
      std::make_unique<StaticTriggerConditions>(
          platform_delegate_, pending_trigger_context_.get(), startup_url),
      std::make_unique<DynamicTriggerConditions>(), ukm_recorder_,
      current_ukm_source_id_);

  // Note: for the duration of the trigger script, the trigger script
  // coordinator will take ownership of the pending trigger context.
  trigger_script_coordinator_->Start(
      startup_url, std::move(pending_trigger_context_),
      base::BindOnce(&Starter::OnTriggerScriptFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void Starter::OnTriggerScriptFinished(
    Metrics::TriggerScriptFinishedState state,
    std::unique_ptr<TriggerContext> trigger_context,
    absl::optional<TriggerScriptProto> trigger_script) {
  // Update caches on error or user-cancel.
  if (trigger_script_coordinator_) {
    std::string domain = url_utils::GetOrganizationIdentifyingDomain(
        trigger_script_coordinator_->GetDeeplink());
    switch (state) {
      case Metrics::TriggerScriptFinishedState::NO_TRIGGER_SCRIPT_AVAILABLE:
      case Metrics::TriggerScriptFinishedState::GET_ACTIONS_FAILED:
        cached_failed_trigger_script_fetches_->Put(domain,
                                                   tick_clock_->NowTicks());
        ClearStaleCacheEntries(
            cached_failed_trigger_script_fetches_,
            tick_clock_->NowTicks() - kMaxFailedTriggerScriptsCacheDuration);
        break;
      case Metrics::TriggerScriptFinishedState::PROMPT_FAILED_CANCEL_SESSION:
        user_denylisted_domains_.Put(domain, tick_clock_->NowTicks());
        ClearStaleCacheEntries(
            &user_denylisted_domains_,
            tick_clock_->NowTicks() - kMaxUserDenylistedCacheDuration);
        break;
      default: {
        auto cache_it = cached_failed_trigger_script_fetches_->Peek(domain);
        if (cache_it != cached_failed_trigger_script_fetches_->end()) {
          cached_failed_trigger_script_fetches_->Erase(cache_it);
        }
        break;
      }
    }
  }

  // Delete the coordinator asynchronously, to give this notification time to
  // end gracefully.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&Starter::DeleteTriggerScriptCoordinator,
                                weak_ptr_factory_.GetWeakPtr()));

  if (state != Metrics::TriggerScriptFinishedState::PROMPT_SUCCEEDED) {
    OnStartDone(/* start_script = */ false);
    return;
  }

  // Take back ownership of the trigger context.
  pending_trigger_context_ = std::move(trigger_context);

  // Note: most trigger scripts show the onboarding on their own and log a
  // different metric for the result. We need to be careful to only run the
  // regular onboarding if necessary to avoid logging metrics more than once.
  if (platform_delegate_->GetOnboardingAccepted()) {
    OnStartDone(/* start_script = */ true, trigger_script);
    return;
  } else {
    MaybeShowOnboarding(trigger_script);
  }
}

void Starter::MaybeShowOnboarding(
    absl::optional<TriggerScriptProto> trigger_script) {
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
      /* use_dialog_onboarding = */ false, *GetPendingTriggerContext(),
      base::BindOnce(&Starter::OnOnboardingFinished,
                     weak_ptr_factory_.GetWeakPtr(), trigger_script));
}

void Starter::OnOnboardingFinished(
    absl::optional<TriggerScriptProto> trigger_script,
    bool shown,
    OnboardingResult result) {
  waiting_for_onboarding_ = false;
  auto intent =
      GetPendingTriggerContext()->GetScriptParameters().GetIntent().value_or(
          std::string());
  switch (result) {
    case OnboardingResult::DISMISSED:
      Metrics::RecordRegularScriptOnboarding(ukm_recorder_,
                                             current_ukm_source_id_,
                                             Metrics::Onboarding::OB_NO_ANSWER);
      Metrics::RecordDropOut(
          Metrics::DropOutReason::ONBOARDING_BACK_BUTTON_CLICKED, intent);
      break;
    case OnboardingResult::REJECTED:
      if (shown) {
        Metrics::RecordRegularScriptOnboarding(
            ukm_recorder_, current_ukm_source_id_,
            Metrics::Onboarding::OB_CANCELLED);
      } else {
        // Should not happen, but it's technically possible. Only OB_NOT_SHOWN
        // will be recorded, since OB_REJECTED is intended to convey explicit
        // user rejection only.
      }
      Metrics::RecordDropOut(Metrics::DropOutReason::DECLINED, intent);
      break;
    case OnboardingResult::NAVIGATION:
      Metrics::RecordRegularScriptOnboarding(ukm_recorder_,
                                             current_ukm_source_id_,
                                             Metrics::Onboarding::OB_NO_ANSWER);
      Metrics::RecordDropOut(Metrics::DropOutReason::ONBOARDING_NAVIGATION,
                             intent);
      break;
    case OnboardingResult::ACCEPTED:
      if (shown) {
        Metrics::RecordRegularScriptOnboarding(
            ukm_recorder_, current_ukm_source_id_,
            Metrics::Onboarding::OB_ACCEPTED);
      } else {
        // This can happen if the onboarding was already accepted and was thus
        // not shown. We will record OB_NOT_SHOWN but not OB_ACCEPTED, as the
        // latter is intended to convey explicit user consent only.
      }
      break;
  }
  Metrics::RecordRegularScriptOnboarding(
      ukm_recorder_, current_ukm_source_id_,
      shown ? Metrics::Onboarding::OB_SHOWN
            : Metrics::Onboarding::OB_NOT_SHOWN);

  if (result != OnboardingResult::ACCEPTED) {
    runtime_manager_->SetUIState(UIState::kNotShown);
    OnStartDone(/* start_script = */ false);
    return;
  }

  // Onboarding is the last step before regular startup.
  platform_delegate_->SetOnboardingAccepted(true);
  pending_trigger_context_->SetOnboardingShown(shown);
  OnStartDone(/* start_script = */ true, trigger_script);
}

void Starter::OnStartDone(bool start_script,
                          absl::optional<TriggerScriptProto> trigger_script) {
  // If a callback is present, we notify that the checks are done instead of
  // directly starting the script with the default UI.
  if (preconditions_checked_callback_) {
    ReportPreconditionsChecked(start_script);
    return;
  }

  if (!start_script) {
    // Catch-all to ensure that after a failed startup attempt we reset the
    // UI state.
    runtime_manager_->SetUIState(platform_delegate_->IsRegularScriptVisible()
                                     ? UIState::kShown
                                     : UIState::kNotShown);
    pending_trigger_context_.reset();
    return;
  }

  auto startup_url =
      StartupUtil().ChooseStartupUrlForIntent(*pending_trigger_context_);
  DCHECK(startup_url.has_value());

  platform_delegate_->StartScriptDefaultUi(
      *startup_url, std::move(pending_trigger_context_), trigger_script);
}

void Starter::ReportPreconditionsChecked(bool start_script) {
  auto startup_url =
      StartupUtil().ChooseStartupUrlForIntent(*pending_trigger_context_);
  std::move(preconditions_checked_callback_)
      .Run(start_script, startup_url, std::move(pending_trigger_context_));
}

void Starter::DeleteTriggerScriptCoordinator() {
  trigger_script_coordinator_.reset();
}

base::WeakPtr<Starter> Starter::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(Starter);

}  // namespace autofill_assistant
