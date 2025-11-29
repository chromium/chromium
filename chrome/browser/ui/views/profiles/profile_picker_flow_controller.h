// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_FLOW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_FLOW_CONTROLLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller_impl.h"
#include "chrome/browser/ui/views/profiles/profile_management_types.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"
#include "components/signin/public/base/signin_buildflags.h"

struct CoreAccountInfo;
class Profile;
class ProfilePickerPostSignInAdapter;
class ForceSigninUIError;

class ProfilePickerFlowController : public ProfileManagementFlowControllerImpl {
 public:
  ProfilePickerFlowController(ProfilePickerWebContentsHost* host,
                              ClearHostClosure clear_host_callback,
                              ProfilePicker::EntryPoint entry_point,
                              const GURL& selected_profile_target_url,
                              const std::string& initial_email = std::string());
  ~ProfilePickerFlowController() override;

  void Init() override;

  void SwitchToSignIn(ProfilePicker::ProfileInfo profile_info,
                      StepSwitchFinishedCallback switch_finished_callback);

  void SwitchToReauth(
      Profile* profile,
      StepSwitchFinishedCallback switch_finished_callback,
      base::OnceCallback<void(const ForceSigninUIError&)> on_error_callback);

  void CancelSigninFlow() override;

  std::u16string GetFallbackAccessibleWindowTitle() const override;

  // Switch to the flow that is shown when the user decides to create a profile
  // without signing in.
  void SwitchToSignedOutPostIdentityFlow(Profile* profile);

  // ProfileManagementFlowControllerImpl:
  void PickProfile(
      const base::FilePath& profile_path,
      ProfilePicker::ProfilePickingArgs args,
      base::OnceCallback<void(bool)> pick_profile_complete_callback) override;

 protected:
  // ProfileManagementFlowControllerImpl
  base::queue<ProfileManagementFlowController::Step> RegisterPostIdentitySteps(
      PostHostClearedCallback post_host_cleared_callback) override;

 private:
  void OnReauthCompleted(
      Profile* profile,
      base::OnceCallback<void(const ForceSigninUIError&)> on_error_callback,
      bool success,
      const ForceSigninUIError& error);

  void OnProfilePickerStepShownReauthError(
      base::OnceCallback<void(const ForceSigninUIError&)> on_error_callback,
      const ForceSigninUIError& error,
      bool switch_step_success);

  std::unique_ptr<ProfilePickerPostSignInAdapter> CreatePostSignInAdapter(
      Profile* signed_in_profile,
      const CoreAccountInfo& account_info,
      std::unique_ptr<content::WebContents> contents) override;

  // Callback after loading a profile and opening a browser.
  void OnSwitchToProfileComplete(
      bool open_settings,
      bool exit_flow_after_profile_picked,
      base::OnceCallback<void(bool)> pick_profile_complete_callback,
      Browser* browser);

  const ProfilePicker::EntryPoint entry_point_;
  const GURL selected_profile_target_url_;

  // Color provided when a profile creation is initiated, that may be used to
  // tint screens of the profile creation flow (currently this only affects the
  // profile type choice screen, which is the one picking the color). It will
  // also be passed to the finishing steps of the profile creation, as a default
  // color choice that the user would be able to override.
  std::optional<SkColor> suggested_profile_color_;

  // TODO(crbug.com/40942098): To be refactored out.
  // This is used to get the web contents that is used in this structure.
  base::WeakPtr<ProfilePickerPostSignInAdapter> weak_post_sign_in_adapter_;

  base::WeakPtr<Profile> created_profile_;

  // Time when the user picked a profile to open, to measure browser startup
  // performance. Only set when the picker is shown on startup.
  base::TimeTicks profile_picked_time_on_startup_;

  // Email to be prefilled in the profile creation flow.
  std::string initial_email_;

  base::WeakPtrFactory<ProfilePickerFlowController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_FLOW_CONTROLLER_H_
