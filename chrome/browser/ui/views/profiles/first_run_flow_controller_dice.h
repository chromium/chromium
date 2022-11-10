// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_FIRST_RUN_FLOW_CONTROLLER_DICE_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_FIRST_RUN_FLOW_CONTROLLER_DICE_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"

class Profile;

// Creates a step to represent the intro. Exposed for testing.
std::unique_ptr<ProfileManagementStepController> CreateIntroStep(
    ProfilePickerWebContentsHost* host,
    base::RepeatingCallback<void(bool sign_in)> choice_callback,
    bool enable_animations);

class FirstRunFlowControllerDice : public ProfileManagementFlowController {
 public:
  // Profile management flow controller that will run the FRE for `profile` in
  // `host`.
  FirstRunFlowControllerDice(ProfilePickerWebContentsHost* host,
                             ClearHostClosure clear_host_callback,
                             Profile* profile);
  ~FirstRunFlowControllerDice() override;

 private:
  void HandleIntroSigninChoice(bool sign_in);

  const raw_ptr<Profile> profile_;
  base::WeakPtrFactory<FirstRunFlowControllerDice> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_FIRST_RUN_FLOW_CONTROLLER_DICE_H_
