// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_OMNIBOX_EVERYWHERE_FLOW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_OMNIBOX_EVERYWHERE_FLOW_CONTROLLER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller.h"

class Profile;

// Profile management flow controller that runs the Omnibox Everywhere version
// of the Profile Picker.
class ProfilePickerOmniboxEverywhereFlowController
    : public ProfileManagementFlowController {
 public:
  ProfilePickerOmniboxEverywhereFlowController(
      ProfilePickerWebContentsHost* host,
      ClearHostClosure clear_host_callback,
      base::OnceCallback<void(Profile*)> picked_profile_callback);
  ~ProfilePickerOmniboxEverywhereFlowController() override;

  // ProfileManagementFlowController:
  void Init() override;
  void PickProfile(
      const base::FilePath& profile_path,
      ProfilePicker::ProfilePickingArgs args,
      base::OnceCallback<void(bool)> pick_profile_complete_callback) override;

 private:
  // ProfileManagementFlowController:
  void CancelSigninFlow() override;

  void OnPickedProfileLoaded(
      base::OnceCallback<void(bool)> pick_profile_complete_callback,
      Profile* profile);

  void Clear();

  base::OnceCallback<void(Profile*)> picked_profile_callback_;

  base::WeakPtrFactory<ProfilePickerOmniboxEverywhereFlowController>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_OMNIBOX_EVERYWHERE_FLOW_CONTROLLER_H_
