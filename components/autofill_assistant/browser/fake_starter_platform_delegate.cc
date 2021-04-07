// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/fake_starter_platform_delegate.h"

namespace autofill_assistant {

FakeStarterPlatformDelegate::FakeStarterPlatformDelegate() = default;
FakeStarterPlatformDelegate::~FakeStarterPlatformDelegate() = default;

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
  num_install_feature_module_called++;
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
  num_show_onboarding_called++;
  std::move(callback).Run(show_onboarding_result_shown, show_onboarding_result);
}

void FakeStarterPlatformDelegate::HideOnboarding() {}

bool FakeStarterPlatformDelegate::GetProactiveHelpSettingEnabled() const {
  return proactive_help_enabled;
}

void FakeStarterPlatformDelegate::SetProactiveHelpSettingEnabled(bool enabled) {
  proactive_help_enabled = enabled;
}

bool FakeStarterPlatformDelegate::GetMakeSearchesAndBrowsingBetterEnabled()
    const {
  return msbb_enabled;
}

}  // namespace autofill_assistant
