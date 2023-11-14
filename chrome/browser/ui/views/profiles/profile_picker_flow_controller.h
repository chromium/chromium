// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_FLOW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_FLOW_CONTROLLER_H_

#include <string>

#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller_impl.h"
#include "chrome/browser/ui/views/profiles/profile_management_types.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"
#include "components/signin/public/base/signin_buildflags.h"

struct CoreAccountInfo;
class Profile;
class ProfilePickerSignedInFlowController;

class ProfilePickerFlowController : public ProfileManagementFlowControllerImpl {
 public:
  ProfilePickerFlowController(ProfilePickerWebContentsHost* host,
                              ClearHostClosure clear_host_callback,
                              ProfilePicker::EntryPoint entry_point);
  ~ProfilePickerFlowController() override;

  void Init(StepSwitchFinishedCallback step_switch_finished_callback) override;

  void SwitchToDiceSignIn(absl::optional<SkColor> profile_color,
                          StepSwitchFinishedCallback switch_finished_callback);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  void SwitchToReauth(Profile* profile,
                      base::OnceCallback<void()> on_error_callback);
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void SwitchToPostSignIn(Profile* signed_in_profile,
                          const CoreAccountInfo& account_info,
                          absl::optional<SkColor> profile_color,
                          std::unique_ptr<content::WebContents> contents);
#endif

  void CancelPostSignInFlow() override;

  std::u16string GetFallbackAccessibleWindowTitle() const override;

  base::FilePath GetSwitchProfilePathOrEmpty() const;

 protected:
  // ProfileManagementFlowControllerImpl
  base::queue<ProfileManagementFlowController::Step> RegisterPostIdentitySteps()
      override;

 private:
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  std::unique_ptr<ProfilePickerDiceSignInProvider> CreateDiceSignInProvider()
      override;

  void OnReauthCompleted(Profile* profile,
                         base::OnceCallback<void()> on_error_callback,
                         bool success);
  void OnProfilePickerStepShownReauthError(
      base::OnceCallback<void()> on_error_callback,
      bool switch_step_success);
#endif

  std::unique_ptr<ProfilePickerSignedInFlowController>
  CreateSignedInFlowController(
      Profile* signed_in_profile,
      const CoreAccountInfo& account_info,
      std::unique_ptr<content::WebContents> contents) override;

  // When `is_continue_callback` is true, the flow should finishing up
  // immediately so that `post_host_cleared_callback` can be executed, without
  // showing other steps.
  void HandleIdentityStepsCompleted(
      PostHostClearedCallback post_host_cleared_callback,
      bool is_continue_callback);

  const ProfilePicker::EntryPoint entry_point_;

  // Color provided when a profile creation is initiated, that may be used to
  // tint screens of the profile creation flow (currently this only affects the
  // profile type choice screen, which is the one picking the color). It will
  // also be passed to the finishing steps of the profile creation, as a default
  // color choice that the user would be able to override.
  absl::optional<SkColor> suggested_profile_color_;

  // TODO(crbug.com/1359352): To be refactored out.
  // This is used for `ProfilePicker::GetSwitchProfilePath()`. The information
  // should ideally be provided to the handler of the profile switch page once
  // its controller is created instead of relying on static calls.
  base::WeakPtr<ProfilePickerSignedInFlowController>
      weak_signed_in_flow_controller_;

  raw_ptr<Profile> created_profile_ = nullptr;
  PostHostClearedCallback post_host_cleared_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_FLOW_CONTROLLER_H_
