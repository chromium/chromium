// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_DESKTOP_STARTER_DELEGATE_DESKTOP_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_DESKTOP_STARTER_DELEGATE_DESKTOP_H_

#include <memory>
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/common_dependencies.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/onboarding_result.h"
#include "components/autofill_assistant/browser/platform_dependencies.h"
#include "components/autofill_assistant/browser/starter_platform_delegate.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/website_login_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill_assistant {

// Platform agnostic StarterPlatformDelegate used to start headless executions.
class StarterDelegateDesktop
    : public StarterPlatformDelegate,
      public content::WebContentsUserData<StarterDelegateDesktop> {
 public:
  ~StarterDelegateDesktop() override;
  StarterDelegateDesktop(const StarterDelegateDesktop&) = delete;
  StarterDelegateDesktop& operator=(const StarterDelegateDesktop&) = delete;

  // Implements StarterPlatformDelegate:
  std::unique_ptr<TriggerScriptCoordinator::UiDelegate>
  CreateTriggerScriptUiDelegate() override;
  std::unique_ptr<ServiceRequestSender> GetTriggerScriptRequestSenderToInject()
      override;
  void StartScriptDefaultUi(
      GURL url,
      std::unique_ptr<TriggerContext> trigger_context,
      const absl::optional<TriggerScriptProto>& trigger_script) override;
  bool IsRegularScriptRunning() const override;
  bool IsRegularScriptVisible() const override;
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
  bool GetIsLoggedIn() override;
  bool GetIsSupervisedUser() override;
  bool GetIsCustomTab() const override;
  bool GetIsWebLayer() const override;
  bool GetIsTabCreatedByGSA() const override;
  std::unique_ptr<AssistantFieldTrialUtil> CreateFieldTrialUtil() override;
  bool IsAttached() override;
  const CommonDependencies* GetCommonDependencies() const override;
  const PlatformDependencies* GetPlatformDependencies() const override;
  base::WeakPtr<StarterPlatformDelegate> GetWeakPtr() override;

 private:
  friend class content::WebContentsUserData<StarterDelegateDesktop>;
  StarterDelegateDesktop(
      content::WebContents* web_contents,
      std::unique_ptr<CommonDependencies> common_dependencies,
      std::unique_ptr<PlatformDependencies> platform_dependencies);

  const std::unique_ptr<CommonDependencies> common_dependencies_;
  const std::unique_ptr<PlatformDependencies> platform_dependencies_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<StarterDelegateDesktop> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_DESKTOP_STARTER_DELEGATE_DESKTOP_H_
