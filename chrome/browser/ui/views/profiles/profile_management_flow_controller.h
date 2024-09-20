// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MANAGEMENT_FLOW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MANAGEMENT_FLOW_CONTROLLER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/profiles/profile_management_types.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"
#include "content/public/browser/web_contents.h"

class Profile;
class ProfileManagementStepController;
class ProfilePickerWebContentsHost;

// Represents an abstract user facing flow related to profile management.
//
// A profile management flow is made of a series of steps, implemented as
// `ProfileManagementStepController`s and owned by this object.
//
// Typical usage starts with calling `Init()` on the instantiated flow, which
// will register and switch to the first step. Then as the user interacts with
// the flow, this controller will handle instantiating and navigating between
// the next steps.
class ProfileManagementFlowController {
 public:
  // TODO(crbug.com/40237131): Split the steps more granularly across
  // logical steps instead of according to implementation details.
  enum class Step {
    kUnknown,
    // Renders the `chrome://profile-picker` app, covering the profile picker,
    // the profile type choice at the beginning of the profile creation
    // flow and the account selection on Lacros.
    kProfilePicker,
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    // Renders the sign in screen on Dice platforms.
    // TODO(crbug.com/40237764): Support the `kAccountSelection` step on
    // Lacros. Picking an account during the `kLacrosSelectAvailableAccount`
    // flow and the profile creation should be implemented as a standalone step.
    kAccountSelection,
    // Moves the rest of the flow to a browser tab so that the user can complete
    // the SAML sign in they started at the previous step.
    kFinishSamlSignin,
    // Renders the reauth page.
    kReauth,
#endif
    // Renders all post-sign in screens: enterprise management consent, profile
    // switch, sync opt-in, etc.
    kPostSignInFlow,

    // Renders the beginning of the First Run Experience.
    kIntro,

    // Renders a default browser promo.
    kDefaultBrowser,

    // Renders the search engine choice screen.
    kSearchEngineChoice,

    kFinishFlow,
  };

  // Creates a flow controller that will start showing UI when `Init()`-ed.
  // `clear_host_callback` will be called if `host` needs to be closed.
  explicit ProfileManagementFlowController(
      ProfilePickerWebContentsHost* host,
      ClearHostClosure clear_host_callback);
  virtual ~ProfileManagementFlowController();

  // Starts the flow by registering and switching to the first step.
  // If `step_switch_finished_callback` is provided, it will be called with
  // `true` when the navigation to the initial step succeeded, or with `false`
  // otherwise.
  virtual void Init(StepSwitchFinishedCallback step_switch_finished_callback =
                        StepSwitchFinishedCallback()) = 0;

  // Instructs a step registered as `step` to be shown.
  // If `step_switch_finished_callback` is provided, it will be called
  // with `true` when the navigation to `step` succeeded, or with
  // `false` otherwise.
  // Also see `ProfileManagementStepController::Show()`.
  void SwitchToStep(Step step,
                    bool reset_state,
                    StepSwitchFinishedCallback step_switch_finished_callback =
                        StepSwitchFinishedCallback(),
                    base::OnceClosure pop_step_callback = base::OnceClosure());

  void OnNavigateBackRequested();

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  void OnReloadRequested();
#endif

  // Cancel the signed-in profile setup and returns back to the main picker
  // screen (if the original EntryPoint was to open the picker).
  virtual void CancelPostSignInFlow() = 0;

  // Clears the current state and reset it to the initial state that shows the
  // main screen. When calling this function the state should not be the
  // initial one. Executes `callback` when the initial state is shown.
  void Reset(StepSwitchFinishedCallback callback);

  // Returns a string to use as title for the window, for accessibility
  // purposes. It is used in case the host is not able to obtain a title from
  // the content it's rendering. As a final fallback, if this value is empty
  // (which is the default), the host will choose itself some generic title.
  virtual std::u16string GetFallbackAccessibleWindowTitle() const;

  // A helper method to create a pop callback that will switch to the given
  // step (can be used with `current_step()` to facilitate switching back to the
  // current active step).
  base::OnceClosure CreateSwitchToStepPopCallback(Step step);

 protected:
  void RegisterStep(Step step,
                    std::unique_ptr<ProfileManagementStepController>);

  void UnregisterStep(Step step);

  bool IsStepInitialized(Step step) const;

  // Closes the flow, calling `clear_host_callback_`, which would cause the
  // `host()` to be deleted.
  void ExitFlow();

  // Opens a browser window for `profile`, closes the flow and then runs
  // `callback` (if it's non-null).
  //
  // Since the flow and its host will be destroyed by the time `callback` runs,
  // it should be longer lived than these.
  void FinishFlowAndRunInBrowser(Profile* profile,
                                 PostHostClearedCallback callback);

  // Will be called at the beginning of `FinishFlowAndRunInBrowser`.
  //
  // Subclasses should override it if they want to perform some additional
  // operations when the flow is closing. If they are going to open a browser
  // themselves, they should return `true`. The default implementation does
  // nothing and returns `false`.
  virtual bool PreFinishWithBrowser();

  Step current_step() const { return current_step_; }

  ProfilePickerWebContentsHost* host() { return host_; }

  // Creates the web contents associated with `profile` and stores them in
  // `signed_out_flow_web_contents_`.
  void CreateSignedOutFlowWebContents(Profile* profile);

  // Returns a pointer to `signed_out_flow_web_contents_`.
  content::WebContents* GetSignedOutFlowWebContents() const;

 private:
  // Called after a browser is open. Clears the host and then runs the callback.
  void CloseHostAndRunCallback(
      PostHostClearedCallback post_host_cleared_callback,
      Browser* browser);

  Step current_step_ = Step::kUnknown;

  // The signed out flow web contents are used in some steps inside
  // `initialized_steps_`. They have to be destroyed after `initialized_steps_`.
  std::unique_ptr<content::WebContents> signed_out_flow_web_contents_;

  raw_ptr<ProfilePickerWebContentsHost> host_;
  ClearHostClosure clear_host_callback_;

  base::flat_map<Step, std::unique_ptr<ProfileManagementStepController>>
      initialized_steps_;

  base::WeakPtrFactory<ProfileManagementFlowController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MANAGEMENT_FLOW_CONTROLLER_H_
