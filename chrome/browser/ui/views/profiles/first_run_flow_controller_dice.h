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
  bool PreFinishWithBrowser() override;

  std::unique_ptr<ProfilePickerDiceSignInProvider> CreateDiceSignInProvider()
      override;

  // `account_info` may not be set as the primary account yet.
  std::unique_ptr<ProfilePickerSignedInFlowController>
  CreateSignedInFlowController(
      Profile* signed_in_profile,
      const CoreAccountInfo& account_info,
      std::unique_ptr<content::WebContents> contents) override;

 private:
  void HandleIntroSigninChoice(IntroChoice choice);

  // To be called when the sign-in and/or sync steps of the flow are completed
  // (or skipped), to proceed with additional steps or finish the flow.
  //
  // When `is_continue_callback` is true, the flow should finishing up
  // immediately so that `post_host_cleared_callback` can be executed, without
  // showing other steps.
  void HandleIdentityStepsCompleted(
      PostHostClearedCallback post_host_cleared_callback,
      bool is_continue_callback = false);

  void MaybeShowDefaultBrowserStep(bool should_show_default_browser_step);

  // Callbacks to be called after checking if the browser is already set as
  // default, in case the verification is completed or in case of timeout.
  void OnDefaultBrowserCheckFinished(
      shell_integration::DefaultWebClientState state);
  void OnDefaultBrowserCheckTimeout();

  const raw_ptr<Profile> profile_;
  ProfilePicker::FirstRunExitedCallback first_run_exited_callback_;

  // Callback that will be run when the whole flow is completed, after the
  // host is cleared.
  PostHostClearedCallback post_host_cleared_callback_;

  base::CancelableOnceClosure default_browser_check_timeout_closure_;
  base::OnceCallback<void(bool)> maybe_show_default_browser_callback_;

  base::WeakPtrFactory<FirstRunFlowControllerDice> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_FIRST_RUN_FLOW_CONTROLLER_DICE_H_
