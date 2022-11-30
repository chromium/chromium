// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/fake_starter_platform_delegate.h"
#include "components/autofill_assistant/browser/mock_assistant_field_trial_util.h"

namespace autofill_assistant {

FakeStarterPlatformDelegate::FakeStarterPlatformDelegate(
    std::unique_ptr<FakeCommonDependencies> fake_common_dependencies)
    : fake_common_dependencies_(std::move(fake_common_dependencies)) {}
FakeStarterPlatformDelegate::~FakeStarterPlatformDelegate() = default;

std::unique_ptr<TriggerScriptCoordinator::UiDelegate>
FakeStarterPlatformDelegate::CreateTriggerScriptUiDelegate() {
  return std::move(trigger_script_ui_delegate_);
}

std::unique_ptr<ServiceRequestSender>
FakeStarterPlatformDelegate::GetTriggerScriptRequestSenderToInject() {
  return std::move(trigger_script_request_sender_for_test_);
}

void FakeStarterPlatformDelegate::StartScriptDefaultUi(
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

bool FakeStarterPlatformDelegate::GetIsLoggedIn() {
  return is_logged_in_;
}

bool FakeStarterPlatformDelegate::GetIsSupervisedUser() {
  return is_supervised_user_;
}

bool FakeStarterPlatformDelegate::GetIsAllowedForMachineLearning() {
  return is_allowed_for_machine_learning_;
}

bool FakeStarterPlatformDelegate::GetIsCustomTab() const {
  return is_custom_tab_;
}

bool FakeStarterPlatformDelegate::GetIsWebLayer() const {
  return is_web_layer_;
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

bool FakeStarterPlatformDelegate::IsAttached() {
  return is_attached_;
}

const FakeCommonDependencies*
FakeStarterPlatformDelegate::GetCommonDependencies() const {
  return fake_common_dependencies_.get();
}

const PlatformDependencies*
FakeStarterPlatformDelegate::GetPlatformDependencies() const {
  return &fake_platform_dependencies_;
}

base::WeakPtr<StarterPlatformDelegate>
FakeStarterPlatformDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace autofill_assistant
