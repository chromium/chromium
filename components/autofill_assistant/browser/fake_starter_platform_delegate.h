// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FAKE_STARTER_PLATFORM_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FAKE_STARTER_PLATFORM_DELEGATE_H_

#include "base/callback.h"
#include "components/autofill_assistant/browser/starter_platform_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

class FakeStarterPlatformDelegate : public StarterPlatformDelegate {
 public:
  FakeStarterPlatformDelegate();
  ~FakeStarterPlatformDelegate() override;

  // Implements StarterPlatformDelegate:
  WebsiteLoginManager* GetWebsiteLoginManager() const override;
  version_info::Channel GetChannel() const override;
  bool GetFeatureModuleInstalled() const override;
  void InstallFeatureModule(
      bool show_ui,
      base::OnceCallback<void(Metrics::FeatureModuleInstallation result)>
          callback) override;
  bool GetIsFirstTimeUser() const override;
  void SetIsFirstTimeUser(bool first_time_user) override;
  bool GetOnboardingAccepted() const override;
  void SetOnboardingAccepted(bool accepted) override;
  void ShowOnboarding(
      bool use_dialog_onboarding,
      const TriggerContext& trigger_context,
      base::OnceCallback<void(bool shown, OnboardingResult result)> callback)
      override;
  void HideOnboarding() override;
  bool GetProactiveHelpSettingEnabled() const override;
  void SetProactiveHelpSettingEnabled(bool enabled) override;
  bool GetMakeSearchesAndBrowsingBetterEnabled() const override;

  WebsiteLoginManager* website_login_manager_ = nullptr;
  version_info::Channel channel_ = version_info::Channel::UNKNOWN;
  bool feature_module_installed_ = true;
  Metrics::FeatureModuleInstallation feature_module_installation_result_ =
      Metrics::FeatureModuleInstallation::DFM_ALREADY_INSTALLED;
  bool is_first_time_user_ = false;
  bool onboarding_accepted_ = true;
  bool show_onboarding_result_shown = false;
  OnboardingResult show_onboarding_result = OnboardingResult::ACCEPTED;
  bool proactive_help_enabled = true;
  bool msbb_enabled = true;

  int num_install_feature_module_called = 0;
  int num_show_onboarding_called = 0;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FAKE_STARTER_PLATFORM_DELEGATE_H_
