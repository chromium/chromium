// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MANAGEMENT_FLOW_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MANAGEMENT_FLOW_CONTROLLER_IMPL_H_

#include <memory>
#include "base/containers/queue.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_types.h"

struct CoreAccountInfo;
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
      const CoreAccountInfo& account_info,
      std::unique_ptr<content::WebContents> contents,
      StepSwitchFinishedCallback step_switch_finished_callback);

  // Move to the steps that come after the identity step.
  void SwitchToPostIdentitySteps();

  virtual std::unique_ptr<ProfilePickerSignedInFlowController>
  CreateSignedInFlowController(
      Profile* signed_in_profile,
      const CoreAccountInfo& account_info,
      std::unique_ptr<content::WebContents> contents) = 0;

  // Register the steps that will be shown after the identity step. The steps
  // should be registered and pushed to the queue in the order in which they
  // should be displayed.
  virtual base::queue<ProfileManagementFlowController::Step>
  RegisterPostIdentitySteps();

  // Switches to the step at the front of the `post_identity_steps_` queue if it
  // is not empty.
  void AdvanceToNextPostIdentityStep();

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Creates, registers and switches to steps to implement the identity flow
  // (signing in then doing the post sign in, which are driven by `Delegate`).
  void SwitchToIdentityStepsFromAccountSelection(
      StepSwitchFinishedCallback step_switch_finished_callback);

  virtual std::unique_ptr<ProfilePickerDiceSignInProvider>
  CreateDiceSignInProvider() = 0;
#endif

 private:
  std::unique_ptr<ProfileManagementStepController> CreatePostSignInStep(
      Profile* signed_in_profile,
      const CoreAccountInfo& account_info,
      std::unique_ptr<content::WebContents> contents);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  std::unique_ptr<ProfileManagementStepController> CreateSamlStep(
      Profile* signed_in_profile,
      std::unique_ptr<content::WebContents> contents);

  // `account_id` is empty is empty if the signin could not complete and must
  // continue in a browser (e.g. for SAML).
  void HandleSignInCompleted(Profile* signed_in_profile,
                             const CoreAccountInfo& account_info,
                             std::unique_ptr<content::WebContents> contents);
#endif

  // The list of steps that are added to the flow.
  // It is populated by the return value of `RegisterPostIdentitySteps` that
  // should be overridden by the derived class.
  // `post_identity_steps` being empty would mean that we either don't have any
  // steps in the flow or that the flow is done.
  base::queue<ProfileManagementFlowController::Step> post_identity_steps_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MANAGEMENT_FLOW_CONTROLLER_IMPL_H_
