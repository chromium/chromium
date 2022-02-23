// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/fake_starter_platform_delegate.h"
#include "components/autofill_assistant/browser/mock_assistant_field_trial_util.h"

namespace autofill_assistant {

FakeStarterPlatformDelegate::FakeStarterPlatformDelegate() = default;
FakeStarterPlatformDelegate::~FakeStarterPlatformDelegate() = default;

std::unique_ptr<TriggerScriptCoordinator::UiDelegate>
FakeStarterPlatformDelegate::CreateTriggerScriptUiDelegate() {
  return std::move(trigger_script_ui_delegate_);
}

std::unique_ptr<ServiceRequestSender>
FakeStarterPlatformDelegate::GetTriggerScriptRequestSenderToInject() {
  return std::move(trigger_script_request_sender_for_test_);
}

void FakeStarterPlatformDelegate::StartRegularScript(
    GURL url,
    std::unique_ptr<TriggerContext> trigger_context,
    const absl::optional<TriggerScriptProto>& trigger_script) {
  if (start_regular_script_callback_) {
    std::move(start_regular_script_callback_)
        .Run(url, std::move(trigger_context), trigger_script);
  }
}

bool FakeStarterPlatformDelegate::IsRegularScriptRunning() const {
  return is_regular_script_running_;
}

bool FakeStarterPlatformDelegate::IsRegularScriptVisible() const {
  return is_regular_script_visible_;
}

WebsiteLoginManager* FakeStarterPlatformDelegate::GetWebsiteLoginManager()
    const {
  return website_login_manager_;
}

version_info::Channel FakeStarterPlatformDelegate::GetChannel() const {
  return channel_;
}

bool FakeStarterPlatformDelegate::GetFeatureModuleInstalled() const {
  return feature_module_installed_;
}

void FakeStarterPlatformDelegate::InstallFeatureModule(
    bool show_ui,
    base::OnceCallback<void(Metrics::FeatureModuleInstallation result)>
        callback) {
  num_install_feature_module_called_++;
  std::move(callback).Run(feature_module_installation_result_);
}

bool FakeStarterPlatformDelegate::GetIsFirstTimeUser() const {
  return is_first_time_user_;
}

void FakeStarterPlatformDelegate::SetIsFirstTimeUser(bool first_time_user) {
  is_first_time_user_ = first_time_user;
}

bool FakeStarterPlatformDelegate::GetOnboardingAccepted() const {
  return onboarding_accepted_;
}

void FakeStarterPlatformDelegate::SetOnboardingAccepted(bool accepted) {
  onboarding_accepted_ = accepted;
}

void FakeStarterPlatformDelegate::ShowOnboarding(
    bool use_dialog_onboarding,
    const TriggerContext& trigger_context,
    base::OnceCallback<void(bool shown, OnboardingResult result)> callback) {
  num_show_onboarding_called_++;
  if (on_show_onboarding_callback_) {
    std::move(on_show_onboarding_callback_).Run(std::move(callback));
    return;
  }
  std::move(callback).Run(show_onboarding_result_shown_,
                          show_onboarding_result_);
}

void FakeStarterPlatformDelegate::HideOnboarding() {}

bool FakeStarterPlatformDelegate::GetProactiveHelpSettingEnabled() const {
  return proactive_help_enabled_;
}

void FakeStarterPlatformDelegate::SetProactiveHelpSettingEnabled(bool enabled) {
  proactive_help_enabled_ = enabled;
}

bool FakeStarterPlatformDelegate::GetMakeSearchesAndBrowsingBetterEnabled()
    const {
  return msbb_enabled_;
}

bool FakeStarterPlatformDelegate::GetIsCustomTab() const {
  return is_custom_tab_;
}

bool FakeStarterPlatformDelegate::GetIsTabCreatedByGSA() const {
  return is_tab_created_by_gsa_;
}

std::unique_ptr<AssistantFieldTrialUtil>
FakeStarterPlatformDelegate::CreateFieldTrialUtil() {
  if (field_trial_util_) {
    return std::move(field_trial_util_);
  }
  return std::make_unique<MockAssistantFieldTrialUtil>();
}

}  // namespace autofill_assistant
