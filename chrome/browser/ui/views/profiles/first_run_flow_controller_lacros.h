// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_FIRST_RUN_FLOW_CONTROLLER_LACROS_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_FIRST_RUN_FLOW_CONTROLLER_LACROS_H_

#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller.h"

class FirstRunFlowControllerLacros : public ProfileManagementFlowController {
 public:
  // Profile management flow controller that will run the FRE for `profile` in
  // `host`.
  // `first_run_exited_callback` is guaranteed to be called when the flow is
  // exited.
  FirstRunFlowControllerLacros(
      ProfilePickerWebContentsHost* host,
      ClearHostClosure clear_host_callback,
      Profile* profile,
      ProfilePicker::DebugFirstRunExitedCallback first_run_exited_callback);

  FirstRunFlowControllerLacros(const FirstRunFlowControllerLacros&) = delete;
  FirstRunFlowControllerLacros& operator=(const FirstRunFlowControllerLacros&) =
      delete;

  ~FirstRunFlowControllerLacros() override;

 private:
  void ExitFlowAndRun(Profile* profile, PostHostClearedCallback callback);
  void MarkSyncConfirmationSeen();

  // Captures the operation that the user expected to run at the time we chose
  // to show them the FRE. When we exit the FRE, we MUST run this. We expect
  // that it will cause a UI for the primary profile to be opened.
  ProfilePicker::DebugFirstRunExitedCallback first_run_exited_callback_;

  // Tracks whether the user got to the last step of the FRE flow.
  bool sync_confirmation_seen_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_FIRST_RUN_FLOW_CONTROLLER_LACROS_H_
