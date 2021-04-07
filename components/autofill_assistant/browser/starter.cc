// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/starter.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "components/autofill_assistant/browser/features.h"

namespace autofill_assistant {

using StartupMode = StartupUtil::StartupMode;

Starter::Starter(content::WebContents* web_contents,
                 StarterPlatformDelegate* platform_delegate,
                 ukm::UkmRecorder* ukm_recorder)
    : content::WebContentsObserver(web_contents),
      platform_delegate_(platform_delegate),
      ukm_recorder_(ukm_recorder) {
  CheckSettings();
}

Starter::~Starter() = default;

void Starter::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!fetch_trigger_scripts_on_navigation_) {
    return;
  }

  // TODO(arbesser): fetch trigger scripts when appropriate.
}

void Starter::CheckSettings() {
  bool proactive_help_setting_enabled =
      platform_delegate_->GetProactiveHelpSettingEnabled();
  bool msbb_setting_enabled =
      platform_delegate_->GetMakeSearchesAndBrowsingBetterEnabled();
  bool feature_module_installed =
      platform_delegate_->GetFeatureModuleInstalled();
  fetch_trigger_scripts_on_navigation_ =
      base::FeatureList::IsEnabled(
          features::kAutofillAssistantInChromeTriggering) &&
      proactive_help_setting_enabled && msbb_setting_enabled;

  // If there is a pending startup, re-check that the settings are still
  // allowing the startup to proceed. If not, cancel the startup.
  if (pending_callback_) {
    StartupMode startup_mode = StartupUtil().ChooseStartupModeForIntent(
        *pending_trigger_context_,
        {msbb_setting_enabled, proactive_help_setting_enabled,
         feature_module_installed});
    switch (startup_mode) {
      case StartupMode::START_BASE64_TRIGGER_SCRIPT:
      case StartupMode::START_RPC_TRIGGER_SCRIPT:
      case StartupMode::START_REGULAR:
        return;
      default:
        CancelPendingStartup();
    }
  }
}

void Starter::Start(std::unique_ptr<TriggerContext> trigger_context,
                    StarterResultCallback callback) {
  DCHECK(trigger_context);
  DCHECK(!trigger_context->GetDirectAction());
  CancelPendingStartup();
  pending_trigger_context_ = std::move(trigger_context);
  pending_callback_ = std::move(callback);

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
      RunCallback(/* start_regular_script = */ false);
      return;
    case StartupMode::START_BASE64_TRIGGER_SCRIPT:
    case StartupMode::START_RPC_TRIGGER_SCRIPT:
    case StartupMode::START_REGULAR:
      MaybeInstallFeatureModule(startup_mode);
      return;
  }
}

void Starter::CancelPendingStartup() {
  if (!pending_callback_) {
    return;
  }
  pending_callback_.Reset();
  pending_trigger_context_ = nullptr;
  platform_delegate_->HideOnboarding();
  // TODO(arbesser): stop trigger script if necessary.
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
  if (result != Metrics::FeatureModuleInstallation::
                    DFM_FOREGROUND_INSTALLATION_SUCCEEDED &&
      result != Metrics::FeatureModuleInstallation::DFM_ALREADY_INSTALLED) {
    RunCallback(/* start_regular_script = */ false);
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
      RunCallback(/* start_regular_script = */ false);
      return;
  }
}

void Starter::StartTriggerScript() {
  // TODO(arbesser): implement this.
  OnTriggerScriptFinished(
      Metrics::LiteScriptFinishedState::LITE_SCRIPT_PROMPT_SUCCEEDED,
      base::nullopt);
}

void Starter::OnTriggerScriptFinished(
    Metrics::LiteScriptFinishedState state,
    base::Optional<TriggerScriptProto> trigger_script) {
  if (state != Metrics::LiteScriptFinishedState::LITE_SCRIPT_PROMPT_SUCCEEDED) {
    RunCallback(/* start_regular_script = */ false);
    return;
  }

  // Note: most trigger scripts show the onboarding on their own and log a
  // different metric for the result. We need to be careful to only run the
  // regular onboarding if necessary to avoid logging metrics more than once.
  if (platform_delegate_->GetOnboardingAccepted()) {
    RunCallback(/* start_regular_script = */ true);
    return;
  } else {
    MaybeShowOnboarding();
  }
}

void Starter::MaybeShowOnboarding() {
  if (platform_delegate_->GetOnboardingAccepted()) {
    OnOnboardingFinished(/* shown = */ false, OnboardingResult::ACCEPTED);
    return;
  }

  // Always use bottom sheet onboarding here. Trigger scripts may show a dialog
  // onboarding, but if we have reached this part, we're already starting the
  // regular script, where we don't offer dialog onboarding.
  platform_delegate_->ShowOnboarding(
      /* use_dialog_onboarding = */ false, *pending_trigger_context_,
      base::BindOnce(&Starter::OnOnboardingFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void Starter::OnOnboardingFinished(bool shown, OnboardingResult result) {
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
    RunCallback(/* start_regular_script = */ false);
    return;
  }

  // Onboarding is the last step before regular startup.
  platform_delegate_->SetOnboardingAccepted(true);
  pending_trigger_context_->SetOnboardingShown(shown);
  RunCallback(/* start_regular_script = */ true);
}

void Starter::RunCallback(bool start_regular_script) {
  DCHECK(pending_callback_);
  if (!start_regular_script) {
    pending_trigger_context_ = nullptr;
    std::move(pending_callback_)
        .Run(/* start_regular_script = */ false, GURL(), nullptr);
    return;
  }

  auto startup_url =
      StartupUtil().ChooseStartupUrlForIntent(*pending_trigger_context_);
  DCHECK(startup_url.has_value());
  std::move(pending_callback_)
      .Run(/* start_regular_script = */ true, *startup_url,
           std::move(pending_trigger_context_));
}

}  // namespace autofill_assistant
