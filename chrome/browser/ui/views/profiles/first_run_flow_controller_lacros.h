// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_FIRST_RUN_FLOW_CONTROLLER_LACROS_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_FIRST_RUN_FLOW_CONTROLLER_LACROS_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller.h"

class LacrosFirstRunSignedInFlowController;

class FirstRunFlowControllerLacros : public ProfileManagementFlowController {
 public:
  FirstRunFlowControllerLacros(
      ProfilePickerWebContentsHost* host,
      Profile* profile,
      ProfilePicker::DebugFirstRunExitedCallback first_run_exited_callback);

  FirstRunFlowControllerLacros(const FirstRunFlowControllerLacros&) = delete;
  FirstRunFlowControllerLacros& operator=(const FirstRunFlowControllerLacros&) =
      delete;

  ~FirstRunFlowControllerLacros() override;

 private:
  void ExitFlowAndRun(ProfilePicker::BrowserOpenedCallback callback);

  // Captures the operation that the user expected to run at the time we chose
  // to show them the FRE. When we complete the FRE, we run this and we expect
  // that it will cause a browser to be opened.
  ProfilePicker::DebugFirstRunExitedCallback first_run_exited_callback_;

  // Gives access to the signed-in flow controller, which is owned by the step.
  // TODO(crbug.com/1358845): Remove it once we can monitor advancement after
  // the first screen as a navigation from chrome://intro.
  base::WeakPtr<LacrosFirstRunSignedInFlowController> signed_in_flow_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_FIRST_RUN_FLOW_CONTROLLER_LACROS_H_
