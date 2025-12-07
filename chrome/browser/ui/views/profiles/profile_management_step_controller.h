// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MANAGEMENT_STEP_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MANAGEMENT_STEP_CONTROLLER_H_

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service.h"
#include "chrome/browser/ui/views/profiles/profile_management_types.h"
#include "chrome/browser/ui/views/profiles/profile_picker_sign_in_provider.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "url/gurl.h"

class ProfilePickerPostSignInAdapter;
class ProfilePickerWebContentsHost;

namespace content {
class WebContents;
}

// Represents a portion of a profile management flow.
// The step controller owns data and helpers necessary for the implementation of
// this specific portion. It also implements shared helpers that make it easier
// to combine steps together and navigate between them..
class ProfileManagementStepController {
 public:
  static std::unique_ptr<ProfileManagementStepController>
  CreateForProfilePickerApp(ProfilePickerWebContentsHost* host,
                            const GURL& initial_url);

  // Forwards the profile and account specific arguments obtained from the
  // sign-in step to the caller, see
  // `ProfilePickerSignInProvider::SignedInCallback` for more info.
  // If a step if shown after this one, the `StepSwitchFinishedCallback` will
  // be called when the new step is shown. Otherwise, it might just be dropped
  // as the host gets cleared.
  using SignInStepFinishedCallback = base::OnceCallback<void(
      Profile*,
      const CoreAccountInfo&,
      std::unique_ptr<content::WebContents>,
      StepSwitchFinishedCallback step_switch_finished_callback)>;

  using SigninErrorCallback = base::OnceCallback<
      void(Profile*, content::WebContents*, const SigninUIError&)>;

  static std::unique_ptr<ProfileManagementStepController> CreateForSignIn(
      ProfilePickerWebContentsHost* host,
      std::unique_ptr<ProfilePickerSignInProvider> sign_in_provider,
      SignInStepFinishedCallback signed_in_callback,
      SigninErrorCallback signin_error_callback);

  // Creates a step controller that will take over from the sign-in step during
  // a SAML sign-in flow, and transition the flow into a browser window where it
  // can be completed. `contents` should be the one used to render the sign-in
  // page. The next steps of the flow will continue in that same `WebContents`.
  // `finish_picker_section_callback` will be called by the controller to
  // request the in-picker flow to be terminated, passing a
  // `PostHostClearedCallback` that should then be executed to resume the flow
  // in a regular browser window.
  static std::unique_ptr<ProfileManagementStepController>
  CreateForFinishSamlSignIn(ProfilePickerWebContentsHost* host,
                            Profile* profile,
                            std::unique_ptr<content::WebContents> contents,
                            base::OnceCallback<void(PostHostClearedCallback)>
                                finish_picker_section_callback);

  static std::unique_ptr<ProfileManagementStepController>
  CreateForPostSignInFlow(
      ProfilePickerWebContentsHost* host,
      std::unique_ptr<ProfilePickerPostSignInAdapter> signed_in_flow);

  static std::unique_ptr<ProfileManagementStepController>
  CreateForSearchEngineChoice(
      ProfilePickerWebContentsHost* host,
      SearchEngineChoiceDialogService* search_engine_choice_dialog_service,
      content::WebContents* web_contents,
      SearchEngineChoiceDialogService::EntryPoint entry_point,
      base::OnceClosure callback);

  // Creates the step that will finish the flow and launch the browser.
  static std::unique_ptr<ProfileManagementStepController>
  CreateForFinishFlowAndRunInBrowser(
      ProfilePickerWebContentsHost* host,
      base::OnceClosure finish_flow_and_run_in_browser_callback);

  explicit ProfileManagementStepController(ProfilePickerWebContentsHost* host);
  virtual ~ProfileManagementStepController();

  // Attempts to show the current step in the `host_`.
  // `step_shown_callback` is not null, and should be executed based on whether
  // the step was shown or skipped.
  // `reset_state` indicates that the step should reset its internal state and
  // appear as freshly created. Callers should pass `true` for newly created
  // steps.
  virtual void Show(StepSwitchFinishedCallback step_shown_callback,
                    bool reset_state) = 0;

  // Frees up unneeded resources. `Show()` will be called if it's needed again.
  virtual void OnHidden() {}

  // Method to be called if the user is attempting to reload this step.
  virtual void OnReloadRequested();

  // Method to be called if the user is attempting to navigate back.
  virtual void OnNavigateBackRequested() = 0;

  void set_pop_step_callback(base::OnceClosure callback) {
    pop_step_callback_ = std::move(callback);
  }

 protected:
  // Returns whether we can navigate back from this step.
  // If it returns true, we expect that a `pop_step_callback_` is set (by
  // calling `set_pop_step_callback()`) before we attempt to navigate back.
  virtual bool CanPopStep() const;

  // Helper to implement back navigations for `OnNavigateBackRequested()`.
  // `contents` is expected to be the `WebContents` in which the current step
  // is rendered, to determine whether some internal history can be popped.
  // If we are allowed to navigate back to the previous step (see
  // `CanPopStep()`), we expect `set_pop_step_callback()` to have been called
  // before.
  void NavigateBackInternal(content::WebContents* contents);

  ProfilePickerWebContentsHost* host() const { return host_; }

 private:
  raw_ptr<ProfilePickerWebContentsHost> host_;

  base::OnceClosure pop_step_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MANAGEMENT_STEP_CONTROLLER_H_
