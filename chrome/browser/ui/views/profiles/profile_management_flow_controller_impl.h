// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MANAGEMENT_FLOW_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MANAGEMENT_FLOW_CONTROLLER_IMPL_H_

#include <memory>
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;
class ProfilePickerWebContentsHost;
class ProfileManagementStepController;
class ProfilePickerSignedInFlowController;

namespace content {
class WebContents;
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class ProfilePickerDiceSignInProvider;
#endif

// Allows sharing the logic for registering and connecting together
// identity-related profile management steps.
class ProfileManagementFlowControllerImpl
    : public ProfileManagementFlowController {
 public:
  ProfileManagementFlowControllerImpl(ProfilePickerWebContentsHost* host,
                                      ClearHostClosure clear_host_callback);
  ~ProfileManagementFlowControllerImpl() override;

 protected:
  void SwitchToIdentityStepsFromPostSignIn(
      Profile* signed_in_profile,
      std::unique_ptr<content::WebContents> contents,
      StepSwitchFinishedCallback step_switch_finished_callback);

  virtual std::unique_ptr<ProfilePickerSignedInFlowController>
  CreateSignedInFlowController(Profile* signed_in_profile,
                               std::unique_ptr<content::WebContents> contents,
                               FinishFlowCallback finish_flow_callback) = 0;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Creates, registers and switches to steps to implement the identity flow
  // (signing in then doing the post sign in, which are driven by `Delegate`).
  void SwitchToIdentityStepsFromAccountSelection(
      StepSwitchFinishedCallback step_switch_finished_callback);

  virtual std::unique_ptr<ProfilePickerDiceSignInProvider>
  CreateDiceSignInProvider() = 0;

  virtual absl::optional<SkColor> GetProfileColor();
#endif

 private:
  std::unique_ptr<ProfileManagementStepController> CreatePostSignInStep(
      Profile* signed_in_profile,
      std::unique_ptr<content::WebContents> contents);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  std::unique_ptr<ProfileManagementStepController> CreateSamlStep(
      Profile* signed_in_profile,
      std::unique_ptr<content::WebContents> contents);

  void HandleSignInCompleted(Profile* signed_in_profile,
                             bool is_saml,
                             std::unique_ptr<content::WebContents> contents);
#endif
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MANAGEMENT_FLOW_CONTROLLER_IMPL_H_
