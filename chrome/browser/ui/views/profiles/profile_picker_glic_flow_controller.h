// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_GLIC_FLOW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_GLIC_FLOW_CONTROLLER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class Profile;

// Profile management flow controller that will run the Glic version of the
// Profile Picker.
class ProfilePickerGlicFlowController
    : public ProfileManagementFlowController,
      public signin::IdentityManager::Observer {
 public:
  // `picked_profile_callback` will always be called, and may be called with
  // a nullptr profile in case the profile failed to load or the `host` was
  // closed without any selection.
  // If the returned `profile` is not null, the controller will ensure that the
  // profile is not destroyed by keeping a `ScopedProfileKeepAlive` during the
  // execution of `picked_profile_callback`, the callback is then expected to
  // set its own if the Profile should not be destroyed.
  ProfilePickerGlicFlowController(
      ProfilePickerWebContentsHost* host,
      ClearHostClosure clear_host_callback,
      base::OnceCallback<void(Profile*)> picked_profile_callback);
  ~ProfilePickerGlicFlowController() override;

  // ProfileManagementFlowController:
  void Init() override;
  // Loads the profile with `profile_path` asynchronously then attempts to run
  // the `picked_profile_callback_` callback with the loaded profile.
  // `args` are actually not used in this implementation.
  void PickProfile(
      const base::FilePath& profile_path,
      ProfilePicker::ProfilePickingArgs args,
      base::OnceCallback<void(bool)> pick_profile_complete_callback) override;

 private:
  // ProfileManagementFlowController:
  void CancelSigninFlow() override;

  // signin::IdentityManager::Observer:
  void OnRefreshTokensLoaded() override;

  // Callback after loading the picked profile. Prepares `loaded_profile_` and
  // attempts to exit the flow with the loaded profile.
  void OnPickedProfileLoaded(
      base::OnceCallback<void(bool)> pick_profile_complete_callback,
      Profile* profile);

  // Returns `loaded_profile_` through `picked_profile_callback_` while ensuring
  // that the refresh tokens are loaded.
  void ExitFlowWithLoadedProfile();

  // Makes sure to clear the current flow and return a nullptr profile to the
  // callback.
  void Clear();

  base::OnceCallback<void(Profile*)> picked_profile_callback_;

  // This value is only set between the time the picked profile is loaded and it
  // is sent through `picked_profile_callback_`. It is nullptr otherwise.
  raw_ptr<Profile> loaded_profile_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_GLIC_FLOW_CONTROLLER_H_
