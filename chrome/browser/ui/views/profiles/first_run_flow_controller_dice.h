// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_FIRST_RUN_FLOW_CONTROLLER_DICE_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_FIRST_RUN_FLOW_CONTROLLER_DICE_H_

#include <memory>

#include "base/cancelable_callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller_impl.h"
#include "chrome/browser/ui/views/profiles/profile_management_types.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"

struct CoreAccountInfo;
enum class IntroChoice;
class Profile;

// Creates a step to represent the intro. Exposed for testing.
std::unique_ptr<ProfileManagementStepController> CreateIntroStep(
    ProfilePickerWebContentsHost* host,
    base::RepeatingCallback<void(IntroChoice)> choice_callback,
    bool enable_animations);

class FirstRunFlowControllerDice : public ProfileManagementFlowControllerImpl {
 public:
  // Profile management flow controller that will run the FRE for `profile` in
  // `host`.
  FirstRunFlowControllerDice(
      ProfilePickerWebContentsHost* host,
      ClearHostClosure clear_host_callback,
      Profile* profile,
      ProfilePicker::FirstRunExitedCallback first_run_exited_callback);
  ~FirstRunFlowControllerDice() override;

  // ProfileManagementFlowControllerImpl:
  void Init(StepSwitchFinishedCallback step_switch_finished_callback) override;
  void CancelPostSignInFlow() override;

 protected:
  // ProfileManagementFlowControllerImpl
  bool PreFinishWithBrowser() override;
  // `account_info` may not be set as the primary account yet.
  std::unique_ptr<ProfilePickerSignedInFlowController>
  CreateSignedInFlowController(
      Profile* signed_in_profile,
      const CoreAccountInfo& account_info,
      std::unique_ptr<content::WebContents> contents) override;
  base::queue<ProfileManagementFlowController::Step> RegisterPostIdentitySteps(
      PostHostClearedCallback post_host_cleared_callback) override;

 private:
  void HandleIntroSigninChoice(IntroChoice choice);

  // Run the `finish_flow_callback_` if it's not empty.
  void RunFinishFlowCallback();

  const raw_ptr<Profile> profile_;
  ProfilePicker::FirstRunExitedCallback first_run_exited_callback_;

  // The callback that will finish the flow and open the browser.
  base::OnceClosure finish_flow_callback_;

  base::WeakPtrFactory<FirstRunFlowControllerDice> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_FIRST_RUN_FLOW_CONTROLLER_DICE_H_
