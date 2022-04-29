// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/trigger_scripts/trigger_script_coordinator.h"

#include <string>

#include "base/numerics/clamped_math.h"
#include "components/autofill_assistant/browser/client_context.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/protocol_utils.h"
#include "components/autofill_assistant/browser/starter_platform_delegate.h"
#include "components/autofill_assistant/browser/url_utils.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "net/http/http_status_code.h"

namespace {

bool IsDialogOnboardingEnabled() {
  return base::FeatureList::IsEnabled(
      autofill_assistant::features::kAutofillAssistantDialogOnboarding);
}

}  // namespace

namespace autofill_assistant {

TriggerScriptCoordinator::TriggerScriptCoordinator(
    base::WeakPtr<StarterPlatformDelegate> starter_delegate,
    content::WebContents* web_contents,
    std::unique_ptr<WebController> web_controller,
    std::unique_ptr<ServiceRequestSender> request_sender,
    const GURL& get_trigger_scripts_server,
    std::unique_ptr<StaticTriggerConditions> static_trigger_conditions,
    std::unique_ptr<DynamicTriggerConditions> dynamic_trigger_conditions,
    ukm::UkmRecorder* ukm_recorder,
    ukm::SourceId deeplink_ukm_source_id)
    : content::WebContentsObserver(web_contents),
      starter_delegate_(starter_delegate),
      ui_delegate_(starter_delegate->CreateTriggerScriptUiDelegate()),
      request_sender_(std::move(request_sender)),
      get_trigger_scripts_server_(get_trigger_scripts_server),
      web_controller_(std::move(web_controller)),
      static_trigger_conditions_(std::move(static_trigger_conditions)),
      dynamic_trigger_conditions_(std::move(dynamic_trigger_conditions)),
      ukm_recorder_(ukm_recorder),
      ukm_source_id_(deeplink_ukm_source_id) {}

TriggerScriptCoordinator::~TriggerScriptCoordinator() = default;

void TriggerScriptCoordinator::Start(
    const GURL& deeplink_url,
    std::unique_ptr<TriggerContext> trigger_context,
    base::OnceCallback<void(Metrics::TriggerScriptFinishedState,
                            std::unique_ptr<TriggerContext>,
                            absl::optional<TriggerScriptProto>)> callback) {
  DCHECK(!callback_);
  callback_ = std::move(callback);
  deeplink_url_ = deeplink_url;
  trigger_context_ = std::move(trigger_context);

  // Note: do not call ClientContext::Update here. We can only send the
  // following approved fields:
  ClientContextProto client_context;
  client_context.mutable_chrome()->set_chrome_version(
      version_info::GetProductNameAndVersionForUserAgent());
  client_context.set_is_in_chrome_triggered(
      trigger_context_->GetInChromeTriggered());

  request_sender_->SendRequest(
      get_trigger_scripts_server_,
      ProtocolUtils::CreateGetTriggerScriptsRequest(
          deeplink_url_, client_context,
          trigger_context_->GetScriptParameters()),
      ServiceRequestSender::AuthMode::API_KEY,
      base::BindOnce(&TriggerScriptCoordinator::OnGetTriggerScripts,
                     weak_ptr_factory_.GetWeakPtr()),
      autofill_assistant::RpcType::GET_TRIGGER_SCRIPTS);
}

void TriggerScriptCoordinator::OnGetTriggerScripts(
    int http_status,
    const std::string& response,
    const ServiceRequestSender::ResponseInfo& response_info) {
  if (http_status != net::HTTP_OK) {
    Stop(Metrics::TriggerScriptFinishedState::GET_ACTIONS_FAILED);
    return;
  }

  trigger_scripts_.clear();
  additional_allowed_domains_.clear();
  absl::optional<int> trigger_condition_timeout_ms;
  int check_interval_ms;
  absl::optional<std::unique_ptr<ScriptParameters>> script_parameters;
  if (!ProtocolUtils::ParseTriggerScripts(
          response, &trigger_scripts_, &additional_allowed_domains_,
          &check_interval_ms, &trigger_condition_timeout_ms,
          &script_parameters)) {
    Stop(Metrics::TriggerScriptFinishedState::GET_ACTIONS_PARSE_ERROR);
    return;
  }
  if (trigger_scripts_.empty()) {
    Stop(Metrics::TriggerScriptFinishedState::NO_TRIGGER_SCRIPT_AVAILABLE);
    return;
  }
  if (script_parameters.has_value()) {
    // Note that we need to merge the new script parameters with the old set
    // (the new values have precedence). This is because not all parameters were
    // sent to the backend in the first place due to privacy considerations.
    (*script_parameters)->MergeWith(trigger_context_->GetScriptParameters());
    trigger_context_->SetScriptParameters(std::move(*script_parameters));
  }
  trigger_condition_check_interval_ = base::Milliseconds(check_interval_ms);
  if (trigger_condition_timeout_ms.has_value()) {
    // Note: add 1 for the initial, not-delayed check.
    initial_trigger_condition_evaluations_ =
        1 + base::ClampCeil<int64_t>(
                base::Milliseconds(*trigger_condition_timeout_ms) /
                trigger_condition_check_interval_);
  } else {
    initial_trigger_condition_evaluations_ = -1;
  }
  remaining_trigger_condition_evaluations_ =
      initial_trigger_condition_evaluations_;

  Metrics::RecordTriggerScriptShownToUser(
      ukm_recorder_, ukm_source_id_,
      TriggerScriptProto::UNSPECIFIED_TRIGGER_UI_TYPE,
      Metrics::TriggerScriptShownToUser::RUNNING);
  ui_delegate_->Attach(this);
  StartCheckingTriggerConditions();
}

void TriggerScriptCoordinator::PerformTriggerScriptAction(
    TriggerScriptProto::TriggerScriptAction action) {
  switch (action) {
    case TriggerScriptProto::NOT_NOW:
      if (visible_trigger_script_ != -1) {
        Metrics::RecordTriggerScriptShownToUser(
            ukm_recorder_, ukm_source_id_, GetTriggerUiTypeForVisibleScript(),
            Metrics::TriggerScriptShownToUser::NOT_NOW);
        trigger_scripts_[visible_trigger_script_]
            ->waiting_for_precondition_no_longer_true(true);
        HideTriggerScript();
      }
      return;
    case TriggerScriptProto::CANCEL_SESSION:
      Stop(Metrics::TriggerScriptFinishedState::PROMPT_FAILED_CANCEL_SESSION);
      return;
    case TriggerScriptProto::CANCEL_FOREVER:
      starter_delegate_->SetProactiveHelpSettingEnabled(false);
      Stop(Metrics::TriggerScriptFinishedState::PROMPT_FAILED_CANCEL_FOREVER);
      return;
    case TriggerScriptProto::SHOW_CANCEL_POPUP:
      // This action is currently performed in Java.
      ui_timeout_timer_.Stop();
      return;
    case TriggerScriptProto::ACCEPT:
      if (visible_trigger_script_ == -1) {
        NOTREACHED();
        return;
      }
      waiting_for_onboarding_ = true;
      ui_timeout_timer_.Stop();
      starter_delegate_->ShowOnboarding(
          IsDialogOnboardingEnabled(), *trigger_context_.get(),
          base::BindOnce(&TriggerScriptCoordinator::OnOnboardingFinished,
                         weak_ptr_factory_.GetWeakPtr()));
      return;
    case TriggerScriptProto::UNDEFINED:
      return;
  }
}

void TriggerScriptCoordinator::OnOnboardingFinished(bool onboardingShown,
                                                    OnboardingResult result) {
  // TODO(b/174445633): Replace -1 with a constant like kTriggerScriptNotVisible
  // at all relevant places
  waiting_for_onboarding_ = false;
  if (visible_trigger_script_ != -1) {
    TriggerScriptProto::TriggerUIType trigger_ui_type =
        GetTriggerUiTypeForVisibleScript();
    if (onboardingShown) {
      switch (result) {
        case OnboardingResult::DISMISSED:
          Metrics::RecordTriggerScriptOnboarding(
              ukm_recorder_, ukm_source_id_, trigger_ui_type,
              Metrics::TriggerScriptOnboarding::ONBOARDING_SEEN_AND_DISMISSED);
          break;
        case OnboardingResult::REJECTED:
          Metrics::RecordTriggerScriptOnboarding(
              ukm_recorder_, ukm_source_id_, trigger_ui_type,
              Metrics::TriggerScriptOnboarding::ONBOARDING_SEEN_AND_REJECTED);
          break;
        case OnboardingResult::NAVIGATION:
          Metrics::RecordTriggerScriptOnboarding(
              ukm_recorder_, ukm_source_id_, trigger_ui_type,
              Metrics::TriggerScriptOnboarding::
                  ONBOARDING_SEEN_AND_INTERRUPTED_BY_NAVIGATION);
          break;
        case OnboardingResult::ACCEPTED:
          Metrics::RecordTriggerScriptOnboarding(
              ukm_recorder_, ukm_source_id_, trigger_ui_type,
              Metrics::TriggerScriptOnboarding::ONBOARDING_SEEN_AND_ACCEPTED);
          break;
      }
    } else {
      Metrics::RecordTriggerScriptOnboarding(
          ukm_recorder_, ukm_source_id_, trigger_ui_type,
          Metrics::TriggerScriptOnboarding::ONBOARDING_ALREADY_ACCEPTED);
    }

    trigger_context_->SetOnboardingShown(onboardingShown);
    if (result == OnboardingResult::ACCEPTED) {
      // Do not hide the trigger script here, to facilitate a smooth
      // transition to the regular flow.
      StopCheckingTriggerConditions();
      starter_delegate_->SetOnboardingAccepted(true);
      ui_delegate_->Detach();
      RunCallback(trigger_ui_type,
                  Metrics::TriggerScriptFinishedState::PROMPT_SUCCEEDED,
                  trigger_scripts_[visible_trigger_script_]->AsProto());
    } else if (!IsDialogOnboardingEnabled()) {
      Stop(
          Metrics::TriggerScriptFinishedState::BOTTOMSHEET_ONBOARDING_REJECTED);
    }
  }
}

void TriggerScriptCoordinator::OnBottomSheetClosedWithSwipe() {
  if (visible_trigger_script_ == -1) {
    NOTREACHED();
    Stop(Metrics::TriggerScriptFinishedState::UNKNOWN_FAILURE);
    return;
  }
  Metrics::RecordTriggerScriptShownToUser(
      ukm_recorder_, ukm_source_id_, GetTriggerUiTypeForVisibleScript(),
      Metrics::TriggerScriptShownToUser::SWIPE_DISMISSED);
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
    Stop(Metrics::TriggerScriptFinishedState::FAILED_TO_SHOW);
    return;
  }
  // Note: do not update the static trigger conditions here! We should ignore
  // this particular update to avoid hiding the first-time trigger script
  // immediately after showing it.
  starter_delegate_->SetIsFirstTimeUser(false);
  if (visible_trigger_script_ != -1 && trigger_scripts_[visible_trigger_script_]
                                           ->AsProto()
                                           .user_interface()
                                           .has_ui_timeout_ms()) {
    ui_timeout_timer_.Start(
        FROM_HERE,
        base::Milliseconds(trigger_scripts_[visible_trigger_script_]
                               ->AsProto()
                               .user_interface()
                               .ui_timeout_ms()),
        base::BindOnce(&TriggerScriptCoordinator::OnUiTimeoutReached,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void TriggerScriptCoordinator::OnUiTimeoutReached() {
  if (visible_trigger_script_ == -1) {
    return;
  }

  Metrics::RecordTriggerScriptShownToUser(
      ukm_recorder_, ukm_source_id_, GetTriggerUiTypeForVisibleScript(),
      Metrics::TriggerScriptShownToUser::UI_TIMEOUT);
  trigger_scripts_[visible_trigger_script_]
      ->waiting_for_precondition_no_longer_true(true);
  HideTriggerScript();
}

void TriggerScriptCoordinator::Stop(Metrics::TriggerScriptFinishedState state) {
  if (!callback_ || !trigger_context_) {
    return;
  }
  VLOG(2) << "Stopping with status " << state;
  TriggerScriptProto::TriggerUIType trigger_ui_type =
      GetTriggerUiTypeForVisibleScript();
  HideTriggerScript();
  StopCheckingTriggerConditions();
  ui_delegate_->Detach();

  if (waiting_for_onboarding_ &&
      state == Metrics::TriggerScriptFinishedState::PROMPT_FAILED_NAVIGATE) {
    starter_delegate_->HideOnboarding();
    Metrics::RecordTriggerScriptOnboarding(
        ukm_recorder_, ukm_source_id_, trigger_ui_type,
        Metrics::TriggerScriptOnboarding::
            ONBOARDING_SEEN_AND_INTERRUPTED_BY_NAVIGATION);
  }
  waiting_for_onboarding_ = false;

  RunCallback(trigger_ui_type, state, /* trigger_script = */ absl::nullopt);
}

void TriggerScriptCoordinator::PrimaryPageChanged(content::Page& page) {
  // Ignore navigation events if any of the following is true:
  // - not currently checking for preconditions (i.e., not yet started).
  if (!is_checking_trigger_conditions_)
    return;

  // A navigation also serves as a boundary for NOT_NOW. This prevents possible
  // race conditions where the UI remains on screen even after navigations.
  for (auto& trigger_script : trigger_scripts_) {
    trigger_script->waiting_for_precondition_no_longer_true(false);
  }

  // Chrome has encountered an error and is now displaying an error message
  // (e.g., network connection lost). This will cancel the current trigger
  // script session.
  if (page.GetMainDocument().IsErrorDocument()) {
    Stop(Metrics::TriggerScriptFinishedState::NAVIGATION_ERROR);
    return;
  }

  // The user has navigated away from the target domain. This will cancel the
  // current trigger script session.
  if (!(url_utils::IsSamePublicSuffixDomain(deeplink_url_, GetCurrentURL()) &&
        url_utils::IsAllowedSchemaTransition(deeplink_url_, GetCurrentURL())) &&
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
    Stop(Metrics::TriggerScriptFinishedState::PROMPT_FAILED_NAVIGATE);
    return;
  }

  ukm_source_id_ = page.GetMainDocument().GetPageUkmSourceId();
  dynamic_trigger_conditions_->SetURL(GetCurrentURL());
  RunOutOfScheduleTriggerConditionCheck();
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

TriggerContext& TriggerScriptCoordinator::GetTriggerContext() const {
  return *trigger_context_;
}

const GURL& TriggerScriptCoordinator::GetDeeplink() const {
  return deeplink_url_;
}

void TriggerScriptCoordinator::OnEffectiveVisibilityChanged() {
  bool visible = web_contents_visible_ && web_contents_interactable_;
  if (visible) {
    // Restore UI on tab switch. NOTE: an arbitrary amount of time can pass
    // between tab-hide and tab-show. It is not guaranteed that the trigger
    // script that was shown before is still available, hence we need to fetch
    // it again.
    DCHECK(visible_trigger_script_ == -1);
    // While the tab was invisible, the user may have disabled proactive help.
    if (!starter_delegate_->GetProactiveHelpSettingEnabled()) {
      Stop(
          Metrics::TriggerScriptFinishedState::DISABLED_PROACTIVE_HELP_SETTING);
      return;
    }
    // Should never happen, this is just a failsafe and to prevent regression.
    if (!trigger_context_) {
      NOTREACHED() << "No trigger context";
      if (callback_) {
        std::move(callback_).Run(Metrics::TriggerScriptFinishedState::CANCELED,
                                 nullptr, absl::nullopt);
      }
      return;
    }
    VLOG(2) << "Restarting after tab became visible again";
    Start(deeplink_url_, std::move(trigger_context_), std::move(callback_));
  } else {
    // Hide UI on tab switch.
    VLOG(2) << "Pausing after tab became invisible or non-interactable";
    StopCheckingTriggerConditions();
    HideTriggerScript();
  }
}

void TriggerScriptCoordinator::WebContentsDestroyed() {
  if (!finished_state_recorded_) {
    Metrics::RecordTriggerScriptFinished(
        ukm_recorder_, ukm_source_id_, GetTriggerUiTypeForVisibleScript(),
        visible_trigger_script_ == -1
            ? Metrics::TriggerScriptFinishedState::
                  WEB_CONTENTS_DESTROYED_WHILE_INVISIBLE
            : Metrics::TriggerScriptFinishedState::
                  WEB_CONTENTS_DESTROYED_WHILE_VISIBLE);
    finished_state_recorded_ = true;
  }
  ui_delegate_->Detach();
}

void TriggerScriptCoordinator::StartCheckingTriggerConditions() {
  is_checking_trigger_conditions_ = true;
  dynamic_trigger_conditions_->ClearConditions();
  for (const auto& trigger_script : trigger_scripts_) {
    dynamic_trigger_conditions_->AddConditionsFromTriggerScript(
        trigger_script->AsProto());
  }
  static_trigger_conditions_->Update(
      base::BindOnce(&TriggerScriptCoordinator::CheckDynamicTriggerConditions,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TriggerScriptCoordinator::CheckDynamicTriggerConditions() {
  dynamic_trigger_conditions_->SetURL(GetCurrentURL());
  dynamic_trigger_conditions_->Update(
      web_controller_.get(),
      base::BindOnce(
          &TriggerScriptCoordinator::OnDynamicTriggerConditionsEvaluated,
          weak_ptr_factory_.GetWeakPtr(),
          /* is_out_of_schedule = */ false,
          /* start_time = */ base::TimeTicks::Now()));
}

void TriggerScriptCoordinator::StopCheckingTriggerConditions() {
  is_checking_trigger_conditions_ = false;
}

void TriggerScriptCoordinator::ShowTriggerScript(int index) {
  if (visible_trigger_script_ == index) {
    return;
  }

  visible_trigger_script_ = index;
  // GetTriggerUiTypeForVisibleScript() requires visible_trigger_script_ to be
  // set first thing.

  Metrics::RecordTriggerScriptShownToUser(
      ukm_recorder_, ukm_source_id_, GetTriggerUiTypeForVisibleScript(),
      Metrics::TriggerScriptShownToUser::SHOWN_TO_USER);
  ui_delegate_->ShowTriggerScript(
      trigger_scripts_[index]->AsProto().user_interface());
}

void TriggerScriptCoordinator::HideTriggerScript() {
  if (visible_trigger_script_ == -1) {
    return;
  }

  // Since the trigger script is now hidden, the timer to track the amount of
  // time a script was invisible is reset.
  remaining_trigger_condition_evaluations_ =
      initial_trigger_condition_evaluations_;
  visible_trigger_script_ = -1;
  ui_delegate_->HideTriggerScript();
  ui_timeout_timer_.Stop();

  // Now that the trigger script is hidden, we may need to update the
  // static trigger conditions. This is done specifically to account for
  // the |is_first_time_user| flag that might have changed.
  static_trigger_conditions_->Update(base::DoNothing());
}

void TriggerScriptCoordinator::OnDynamicTriggerConditionsEvaluated(
    bool is_out_of_schedule,
    absl::optional<base::TimeTicks> start_time) {
  if (!web_contents_visible_ || !is_checking_trigger_conditions_) {
    return;
  }
  if (!static_trigger_conditions_->has_results() ||
      !dynamic_trigger_conditions_->HasResults()) {
    DCHECK(is_out_of_schedule);
    return;
  }

  if (start_time.has_value()) {
    Metrics::RecordTriggerConditionEvaluationTime(
        ukm_recorder_, ukm_source_id_, base::TimeTicks::Now() - *start_time);
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
    Metrics::RecordTriggerScriptShownToUser(
        ukm_recorder_, ukm_source_id_, GetTriggerUiTypeForVisibleScript(),
        Metrics::TriggerScriptShownToUser::
            HIDE_ON_TRIGGER_CONDITION_NO_LONGER_TRUE);
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
    Stop(Metrics::TriggerScriptFinishedState::TRIGGER_CONDITION_TIMEOUT);
    return;
  }
  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TriggerScriptCoordinator::CheckDynamicTriggerConditions,
                     weak_ptr_factory_.GetWeakPtr()),
      trigger_condition_check_interval_);
}

void TriggerScriptCoordinator::RunOutOfScheduleTriggerConditionCheck() {
  OnDynamicTriggerConditionsEvaluated(/* is_out_of_schedule = */ true,
                                      /* start_time = */ absl::nullopt);
}

void TriggerScriptCoordinator::RunCallback(
    TriggerScriptProto::TriggerUIType trigger_ui_type,
    Metrics::TriggerScriptFinishedState state,
    const absl::optional<TriggerScriptProto>& trigger_script) {
  DCHECK(callback_);
  DCHECK(trigger_context_);
  if (!finished_state_recorded_) {
    finished_state_recorded_ = true;
    Metrics::RecordTriggerScriptFinished(ukm_recorder_, ukm_source_id_,
                                         trigger_ui_type, state);
  }
  trigger_context_->SetTriggerUIType(trigger_ui_type);

  // Prevent notifications after the callback was run, i.e., after
  // trigger_context_ was moved out of this object.
  Observe(nullptr);
  ui_delegate_->Detach();

  std::move(callback_).Run(state, std::move(trigger_context_), trigger_script);
}

TriggerScriptProto::TriggerUIType
TriggerScriptCoordinator::GetTriggerUiTypeForVisibleScript() const {
  if (visible_trigger_script_ >= 0 &&
      static_cast<size_t>(visible_trigger_script_) < trigger_scripts_.size()) {
    return trigger_scripts_[visible_trigger_script_]->trigger_ui_type();
  }
  return TriggerScriptProto::UNSPECIFIED_TRIGGER_UI_TYPE;
}

GURL TriggerScriptCoordinator::GetCurrentURL() const {
  GURL current_url = web_contents()->GetMainFrame()->GetLastCommittedURL();
  if (current_url.is_empty()) {
    return deeplink_url_;
  }
  return current_url;
}

}  // namespace autofill_assistant
