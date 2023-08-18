// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_FIRST_RUN_FLOW_CONTROLLER_LACROS_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_FIRST_RUN_FLOW_CONTROLLER_LACROS_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller_impl.h"
#include "google_apis/gaia/core_account_id.h"

class FirstRunFlowControllerLacros
    : public ProfileManagementFlowControllerImpl {
 public:
  // Profile management flow controller that will run the FRE for `profile` in
  // `host`.
  // `first_run_exited_callback` is guaranteed to be called when the flow is
  // exited.
  FirstRunFlowControllerLacros(
      ProfilePickerWebContentsHost* host,
      ClearHostClosure clear_host_callback,
      Profile* profile,
      ProfilePicker::FirstRunExitedCallback first_run_exited_callback);

  FirstRunFlowControllerLacros(const FirstRunFlowControllerLacros&) = delete;
  FirstRunFlowControllerLacros& operator=(const FirstRunFlowControllerLacros&) =
      delete;

  ~FirstRunFlowControllerLacros() override;

  // ProfileManagementFlowControllerImpl:
  void Init(StepSwitchFinishedCallback step_switch_finished_callback) override;
  void CancelPostSignInFlow() override;

 protected:
  bool PreFinishWithBrowser() override;

  std::unique_ptr<ProfilePickerSignedInFlowController>
  CreateSignedInFlowController(
      Profile* signed_in_profile,
      const CoreAccountId& account_id,
      std::unique_ptr<content::WebContents> contents) override;

 private:
  void MarkSyncConfirmationSeen();

  // Pointer to the primary profile. Safe to keep, in particular we are going to
  // register a profile keep alive through the step we create in `Init()`.
  const raw_ptr<Profile> profile_;

  // Captures the operation that the user expected to run at the time we chose
  // to show them the FRE. When we exit the FRE, we MUST run this. We expect
  // that it will cause a UI for the primary profile to be opened.
  ProfilePicker::FirstRunExitedCallback first_run_exited_callback_;

  // Tracks whether the user got to the last step of the FRE flow.
  bool sync_confirmation_seen_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_FIRST_RUN_FLOW_CONTROLLER_LACROS_H_
