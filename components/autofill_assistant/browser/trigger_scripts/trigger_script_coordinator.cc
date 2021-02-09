// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/trigger_scripts/trigger_script_coordinator.h"

#include <map>

#include "base/numerics/clamped_math.h"
#include "components/autofill_assistant/browser/client_context.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/protocol_utils.h"
#include "components/autofill_assistant/browser/url_utils.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "net/http/http_status_code.h"

namespace {

const char kScriptParameterDebugBundleId[] = "DEBUG_BUNDLE_ID";
const char kScriptParameterDebugBundleVersion[] = "DEBUG_BUNDLE_VERSION";
const char kScriptParameterDebugSocketId[] = "DEBUG_SOCKET_ID";

std::map<std::string, std::string> ExtractDebugScriptParameters(
    const autofill_assistant::TriggerContext& trigger_context) {
  std::map<std::string, std::string> debug_script_parameters;
  auto debug_bundle_id =
      trigger_context.GetParameter(kScriptParameterDebugBundleId);
  auto debug_bundle_version =
      trigger_context.GetParameter(kScriptParameterDebugBundleVersion);
  auto debug_socket_id =
      trigger_context.GetParameter(kScriptParameterDebugSocketId);

  if (debug_bundle_id) {
    debug_script_parameters.insert(
        {kScriptParameterDebugBundleId, *debug_bundle_id});
  }
  if (debug_bundle_version) {
    debug_script_parameters.insert(
        {kScriptParameterDebugBundleVersion, *debug_bundle_version});
  }
  if (debug_socket_id) {
    debug_script_parameters.insert(
        {kScriptParameterDebugSocketId, *debug_socket_id});
  }
  return debug_script_parameters;
}

bool IsDialogOnboardingEnabled() {
  return base::FeatureList::IsEnabled(
      autofill_assistant::features::kAutofillAssistantDialogOnboarding);
}

}  // namespace

namespace autofill_assistant {

TriggerScriptCoordinator::TriggerScriptCoordinator(
    content::WebContents* web_contents,
    WebsiteLoginManager* website_login_manager,
    base::RepeatingCallback<bool(void)> is_first_time_user_callback,
    std::unique_ptr<WebController> web_controller,
    std::unique_ptr<ServiceRequestSender> request_sender,
    const GURL& get_trigger_scripts_server,
    std::unique_ptr<StaticTriggerConditions> static_trigger_conditions,
    std::unique_ptr<DynamicTriggerConditions> dynamic_trigger_conditions,
    ukm::UkmRecorder* ukm_recorder)
    : content::WebContentsObserver(web_contents),
      website_login_manager_(website_login_manager),
      is_first_time_user_callback_(std::move(is_first_time_user_callback)),
      request_sender_(std::move(request_sender)),
      get_trigger_scripts_server_(get_trigger_scripts_server),
      web_controller_(std::move(web_controller)),
      static_trigger_conditions_(std::move(static_trigger_conditions)),
      dynamic_trigger_conditions_(std::move(dynamic_trigger_conditions)),
      ukm_recorder_(ukm_recorder) {}

TriggerScriptCoordinator::~TriggerScriptCoordinator() = default;

void TriggerScriptCoordinator::Start(
    const GURL& deeplink_url,
    std::unique_ptr<TriggerContext> trigger_context) {
  deeplink_url_ = deeplink_url;
  trigger_context_ = std::make_unique<TriggerContextImpl>(
      ExtractDebugScriptParameters(*trigger_context),
      trigger_context->experiment_ids());
  ClientContextProto client_context;
  client_context.mutable_chrome()->set_chrome_version(
      version_info::GetProductNameAndVersionForUserAgent());

  request_sender_->SendRequest(
      get_trigger_scripts_server_,
      ProtocolUtils::CreateGetTriggerScriptsRequest(
          deeplink_url_, client_context, trigger_context_->GetParameters()),
      base::BindOnce(&TriggerScriptCoordinator::OnGetTriggerScripts,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TriggerScriptCoordinator::OnGetTriggerScripts(
    int http_status,
    const std::string& response) {
  if (http_status != net::HTTP_OK) {
    Stop(Metrics::LiteScriptFinishedState::LITE_SCRIPT_GET_ACTIONS_FAILED);
    return;
  }

  trigger_scripts_.clear();
  additional_allowed_domains_.clear();
  base::Optional<int> timeout_ms;
  int check_interval_ms;
  if (!ProtocolUtils::ParseTriggerScripts(response, &trigger_scripts_,
                                          &additional_allowed_domains_,
                                          &check_interval_ms, &timeout_ms)) {
    Stop(Metrics::LiteScriptFinishedState::LITE_SCRIPT_GET_ACTIONS_PARSE_ERROR);
    return;
  }
  if (trigger_scripts_.empty()) {
    Stop(Metrics::LiteScriptFinishedState::
             LITE_SCRIPT_NO_TRIGGER_SCRIPT_AVAILABLE);
    return;
  }
  trigger_condition_check_interval_ =
      base::TimeDelta::FromMilliseconds(check_interval_ms);
  if (timeout_ms.has_value()) {
    // Note: add 1 for the initial, not-delayed check.
    initial_trigger_condition_evaluations_ =
        1 + base::ClampCeil<int64_t>(
                base::TimeDelta::FromMilliseconds(*timeout_ms) /
                trigger_condition_check_interval_);
  } else {
    initial_trigger_condition_evaluations_ = -1;
  }
  remaining_trigger_condition_evaluations_ =
      initial_trigger_condition_evaluations_;

  Metrics::RecordLiteScriptShownToUser(
      ukm_recorder_, web_contents(),
      Metrics::LiteScriptShownToUser::LITE_SCRIPT_RUNNING);
  StartCheckingTriggerConditions();
}

void TriggerScriptCoordinator::PerformTriggerScriptAction(
    TriggerScriptProto::TriggerScriptAction action) {
  switch (action) {
    case TriggerScriptProto::NOT_NOW:
      if (visible_trigger_script_ != -1) {
        Metrics::RecordLiteScriptShownToUser(
            ukm_recorder_, web_contents(),
            Metrics::LiteScriptShownToUser::LITE_SCRIPT_NOT_NOW);
        trigger_scripts_[visible_trigger_script_]
            ->waiting_for_precondition_no_longer_true(true);
        HideTriggerScript();
      }
      return;
    case TriggerScriptProto::CANCEL_SESSION:
      Stop(Metrics::LiteScriptFinishedState::
               LITE_SCRIPT_PROMPT_FAILED_CANCEL_SESSION);
      return;
    case TriggerScriptProto::CANCEL_FOREVER:
      Stop(Metrics::LiteScriptFinishedState::
               LITE_SCRIPT_PROMPT_FAILED_CANCEL_FOREVER);
      return;
    case TriggerScriptProto::SHOW_CANCEL_POPUP:
      NOTREACHED();
      return;
    case TriggerScriptProto::ACCEPT:
      if (visible_trigger_script_ == -1) {
        NOTREACHED();
        return;
      }
      for (Observer& observer : observers_) {
        observer.OnOnboardingRequested(IsDialogOnboardingEnabled());
      }
      return;
    case TriggerScriptProto::UNDEFINED:
      return;
  }
}

void TriggerScriptCoordinator::OnOnboardingFinished(bool onboardingShown,
                                                    OnboardingResult result) {
  // TODO(b/174445633): Replace -1 with a constant like kTriggerScriptNotVisible
  // at all relevant places
  if (visible_trigger_script_ != -1) {
    if (onboardingShown) {
      switch (result) {
        case OnboardingResult::DISMISSED:
          Metrics::RecordLiteScriptOnboarding(
              ukm_recorder_, web_contents(),
              Metrics::LiteScriptOnboarding::
                  LITE_SCRIPT_ONBOARDING_SEEN_AND_DISMISSED);
          break;
        case OnboardingResult::REJECTED:
          Metrics::RecordLiteScriptOnboarding(
              ukm_recorder_, web_contents(),
              Metrics::LiteScriptOnboarding::
                  LITE_SCRIPT_ONBOARDING_SEEN_AND_REJECTED);
          break;
        case OnboardingResult::NAVIGATION:
          Metrics::RecordLiteScriptOnboarding(
              ukm_recorder_, web_contents(),
              Metrics::LiteScriptOnboarding::
                  LITE_SCRIPT_ONBOARDING_SEEN_AND_INTERRUPTED_BY_NAVIGATION);
          break;
        case OnboardingResult::ACCEPTED:
          Metrics::RecordLiteScriptOnboarding(
              ukm_recorder_, web_contents(),
              Metrics::LiteScriptOnboarding::
                  LITE_SCRIPT_ONBOARDING_SEEN_AND_ACCEPTED);
          break;
      }
    } else {
      Metrics::RecordLiteScriptOnboarding(
          ukm_recorder_, web_contents(),
          Metrics::LiteScriptOnboarding::
              LITE_SCRIPT_ONBOARDING_ALREADY_ACCEPTED);
    }

    if (result == OnboardingResult::ACCEPTED) {
      // Do not hide the trigger script here, to facilitate a smooth
      // transition to the regular flow.
      StopCheckingTriggerConditions();
      NotifyOnTriggerScriptFinished(
          Metrics::LiteScriptFinishedState::LITE_SCRIPT_PROMPT_SUCCEEDED);
    } else if (!IsDialogOnboardingEnabled()) {
      Stop(Metrics::LiteScriptFinishedState::
               LITE_SCRIPT_BOTTOMSHEET_ONBOARDING_REJECTED);
    }
  }
}

void TriggerScriptCoordinator::OnBottomSheetClosedWithSwipe() {
  if (visible_trigger_script_ == -1) {
    NOTREACHED();
    Stop(Metrics::LiteScriptFinishedState::LITE_SCRIPT_UNKNOWN_FAILURE);
    return;
  }
  Metrics::RecordLiteScriptShownToUser(
      ukm_recorder_, web_contents(),
      Metrics::LiteScriptShownToUser::LITE_SCRIPT_SWIPE_DISMISSED);
  PerformTriggerScriptAction(trigger_scripts_[visible_trigger_script_]
                                 ->AsProto()
                                 .on_swipe_to_dismiss());
}

bool TriggerScriptCoordinator::OnBackButtonPressed() {
  if (visible_trigger_script_ == -1) {
    return false;
  }
  if (web_contents()->GetController().CanGoBack()) {
    web_contents()->GetController().GoBack();
  }
  // We need to handle this event, because by default the bottom sheet will
  // close when the back button is pressed.
  return true;
}

void TriggerScriptCoordinator::OnKeyboardVisibilityChanged(bool visible) {
  dynamic_trigger_conditions_->SetKeyboardVisible(visible);
  RunOutOfScheduleTriggerConditionCheck();
}

void TriggerScriptCoordinator::OnTriggerScriptShown(bool success) {
  if (!success) {
    Stop(Metrics::LiteScriptFinishedState::LITE_SCRIPT_FAILED_TO_SHOW);
    return;
  }
}

void TriggerScriptCoordinator::OnProactiveHelpSettingChanged(
    bool proactive_help_enabled) {
  if (!proactive_help_enabled) {
    Stop(Metrics::LiteScriptFinishedState::
             LITE_SCRIPT_DISABLED_PROACTIVE_HELP_SETTING);
    return;
  }
}

void TriggerScriptCoordinator::Stop(Metrics::LiteScriptFinishedState state) {
  VLOG(2) << "Stopping with status " << state;
  HideTriggerScript();
  StopCheckingTriggerConditions();
  NotifyOnTriggerScriptFinished(state);
}

void TriggerScriptCoordinator::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TriggerScriptCoordinator::RemoveObserver(const Observer* observer) {
  observers_.RemoveObserver(observer);
}

void TriggerScriptCoordinator::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Ignore navigation events if any of the following is true:
  // - not currently checking for preconditions (i.e., not yet started).
  // - not in the main frame.
  // - document does not change (e.g., same page history navigation).
  // - WebContents stays at the existing URL (e.g., downloads).
  if (!is_checking_trigger_conditions_ || !navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  // Chrome has encountered an error and is now displaying an error message
  // (e.g., network connection lost). This will cancel the current trigger
  // script session.
  if (navigation_handle->IsErrorPage()) {
    Stop(Metrics::LiteScriptFinishedState::LITE_SCRIPT_NAVIGATION_ERROR);
    return;
  }

  // The user has navigated away from the target domain. This will cancel the
  // current trigger script session.
  if (!url_utils::IsInDomainOrSubDomain(GetCurrentURL(), deeplink_url_) &&
      !url_utils::IsInDomainOrSubDomain(GetCurrentURL(),
                                        additional_allowed_domains_)) {
#ifndef NDEBUG
    VLOG(2) << "Unexpected navigation to " << GetCurrentURL();
    VLOG(2) << "List of allowed domains:";
    VLOG(2) << "\t" << deeplink_url_.host();
    for (const auto& domain : additional_allowed_domains_) {
      VLOG(2) << "\t" << domain;
    }
#endif
    Stop(Metrics::LiteScriptFinishedState::LITE_SCRIPT_PROMPT_FAILED_NAVIGATE);
    return;
  }
}

void TriggerScriptCoordinator::OnVisibilityChanged(
    content::Visibility visibility) {
  bool visible = visibility == content::Visibility::VISIBLE;
  if (web_contents_visible_ == visible) {
    return;
  }
  web_contents_visible_ = visible;
  OnEffectiveVisibilityChanged();
}

void TriggerScriptCoordinator::OnTabInteractabilityChanged(bool interactable) {
  if (web_contents_interactable_ == interactable) {
    return;
  }
  web_contents_interactable_ = interactable;
  OnEffectiveVisibilityChanged();
}

void TriggerScriptCoordinator::OnEffectiveVisibilityChanged() {
  bool visible = web_contents_visible_ && web_contents_interactable_;
  if (visible) {
    // Restore UI on tab switch. NOTE: an arbitrary amount of time can pass
    // between tab-hide and tab-show. It is not guaranteed that the trigger
    // script that was shown before is still available, hence we need to fetch
    // it again.
    DCHECK(visible_trigger_script_ == -1);
    VLOG(2) << "Restarting after tab became visible again";
    Start(deeplink_url_, std::move(trigger_context_));
  } else {
    // Hide UI on tab switch.
    VLOG(2) << "Pausing after tab became invisible or non-interactable";
    StopCheckingTriggerConditions();
    HideTriggerScript();
  }

  for (Observer& observer : observers_) {
    observer.OnVisibilityChanged(visible);
  }
}

void TriggerScriptCoordinator::WebContentsDestroyed() {
  if (!finished_state_recorded_) {
    Metrics::RecordLiteScriptFinished(
        ukm_recorder_, web_contents(),
        visible_trigger_script_ == -1
            ? Metrics::LiteScriptFinishedState::
                  LITE_SCRIPT_WEB_CONTENTS_DESTROYED_WHILE_INVISIBLE
            : Metrics::LiteScriptFinishedState::
                  LITE_SCRIPT_WEB_CONTENTS_DESTROYED_WHILE_VISIBLE);
    finished_state_recorded_ = true;
  }
}

void TriggerScriptCoordinator::StartCheckingTriggerConditions() {
  is_checking_trigger_conditions_ = true;
  dynamic_trigger_conditions_->ClearSelectors();
  for (const auto& trigger_script : trigger_scripts_) {
    dynamic_trigger_conditions_->AddSelectorsFromTriggerScript(
        trigger_script->AsProto());
  }
  static_trigger_conditions_->Init(
      website_login_manager_, is_first_time_user_callback_, deeplink_url_,
      trigger_context_.get(),
      base::BindOnce(&TriggerScriptCoordinator::CheckDynamicTriggerConditions,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TriggerScriptCoordinator::CheckDynamicTriggerConditions() {
  dynamic_trigger_conditions_->Update(
      web_controller_.get(),
      base::BindOnce(
          &TriggerScriptCoordinator::OnDynamicTriggerConditionsEvaluated,
          weak_ptr_factory_.GetWeakPtr(),
          /* is_out_of_schedule = */ false));
}

void TriggerScriptCoordinator::StopCheckingTriggerConditions() {
  is_checking_trigger_conditions_ = false;
}

void TriggerScriptCoordinator::ShowTriggerScript(int index) {
  if (visible_trigger_script_ == index) {
    return;
  }

  Metrics::RecordLiteScriptShownToUser(
      ukm_recorder_, web_contents(),
      Metrics::LiteScriptShownToUser::LITE_SCRIPT_SHOWN_TO_USER);
  visible_trigger_script_ = index;
  auto proto = trigger_scripts_[index]->AsProto().user_interface();
  for (Observer& observer : observers_) {
    observer.OnTriggerScriptShown(proto);
  }
}

void TriggerScriptCoordinator::HideTriggerScript() {
  if (visible_trigger_script_ == -1) {
    return;
  }

  // Since the trigger script is now hidden, the timer to track the amount of
  // time a script was invisible is reset.
  remaining_trigger_condition_evaluations_ =
      initial_trigger_condition_evaluations_;
  static_trigger_conditions_->set_is_first_time_user(false);
  visible_trigger_script_ = -1;
  for (Observer& observer : observers_) {
    observer.OnTriggerScriptHidden();
  }
}

void TriggerScriptCoordinator::OnDynamicTriggerConditionsEvaluated(
    bool is_out_of_schedule) {
  if (!web_contents_visible_ || !is_checking_trigger_conditions_) {
    return;
  }
  if (!static_trigger_conditions_->has_results() ||
      !dynamic_trigger_conditions_->HasResults()) {
    DCHECK(is_out_of_schedule);
    return;
  }

  VLOG(3) << "Evaluating trigger conditions...";
  std::vector<bool> evaluated_trigger_conditions;
  for (const auto& trigger_script : trigger_scripts_) {
    evaluated_trigger_conditions.emplace_back(
        trigger_script->EvaluateTriggerConditions(
            *static_trigger_conditions_, *dynamic_trigger_conditions_));
  }

  // Trigger condition for the currently shown trigger script is no longer true.
  if (visible_trigger_script_ != -1 &&
      !evaluated_trigger_conditions[visible_trigger_script_]) {
    Metrics::RecordLiteScriptShownToUser(
        ukm_recorder_, web_contents(),
        Metrics::LiteScriptShownToUser::
            LITE_SCRIPT_HIDE_ON_TRIGGER_CONDITION_NO_LONGER_TRUE);
    HideTriggerScript();
    // Do not return here: a different trigger script may have become eligible
    // at the same time.
  }

  for (size_t i = 0; i < trigger_scripts_.size(); ++i) {
    // The currently visible trigger script is still visible, nothing to do.
    if (visible_trigger_script_ != -1 &&
        i == static_cast<size_t>(visible_trigger_script_) &&
        evaluated_trigger_conditions[i]) {
      DCHECK(!trigger_scripts_[i]->waiting_for_precondition_no_longer_true());
      continue;
    }

    // The script was waiting for the precondition to no longer be true.
    // It can now resume regular precondition checking.
    if (!evaluated_trigger_conditions[i] &&
        trigger_scripts_[i]->waiting_for_precondition_no_longer_true()) {
      trigger_scripts_[i]->waiting_for_precondition_no_longer_true(false);
      continue;
    }

    if (evaluated_trigger_conditions[i] && visible_trigger_script_ != -1 &&
        i != static_cast<size_t>(visible_trigger_script_)) {
      // Should not happen, as trigger script conditions should be mutually
      // exclusive. If it happens, we just ignore it. This is essentially
      // first-come-first-serve, prioritizing scripts w.r.t. occurrence in the
      // proto.
      continue;
    }

    // A new trigger script has become eligible for showing.
    if (evaluated_trigger_conditions[i] &&
        !trigger_scripts_[i]->waiting_for_precondition_no_longer_true()) {
      ShowTriggerScript(i);
    }
  }

  if (is_out_of_schedule) {
    // Out-of-schedule checks do not count towards the timeout.
    return;
  }
  if (visible_trigger_script_ == -1 &&
      remaining_trigger_condition_evaluations_ > 0) {
    remaining_trigger_condition_evaluations_--;
  }
  if (remaining_trigger_condition_evaluations_ == 0) {
    Stop(Metrics::LiteScriptFinishedState::
             LITE_SCRIPT_TRIGGER_CONDITION_TIMEOUT);
    return;
  }
  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TriggerScriptCoordinator::CheckDynamicTriggerConditions,
                     weak_ptr_factory_.GetWeakPtr()),
      trigger_condition_check_interval_);
}

void TriggerScriptCoordinator::RunOutOfScheduleTriggerConditionCheck() {
  OnDynamicTriggerConditionsEvaluated(/* is_out_of_schedule = */ true);
}

void TriggerScriptCoordinator::NotifyOnTriggerScriptFinished(
    Metrics::LiteScriptFinishedState state) {
  if (!finished_state_recorded_) {
    finished_state_recorded_ = true;
    Metrics::RecordLiteScriptFinished(ukm_recorder_, web_contents(), state);
  }

  for (Observer& observer : observers_) {
    observer.OnTriggerScriptFinished(state);
  }
}

GURL TriggerScriptCoordinator::GetCurrentURL() const {
  GURL current_url = web_contents()->GetLastCommittedURL();
  if (current_url.is_empty()) {
    return deeplink_url_;
  }
  return current_url;
}

}  // namespace autofill_assistant
