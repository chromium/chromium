// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/first_run_flow_controller_lacros.h"

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/profiles/lacros_first_run_signed_in_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"

namespace {

// Helper to run `callback`, after hiding the profile picker.
void HideProfilePickerAndRun(ProfilePicker::BrowserOpenedCallback callback) {
  ProfilePicker::Hide();

  if (!callback)
    return;

  // See if there is already a browser we can use.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile = profile_manager->GetProfileByPath(
      profile_manager->GetPrimaryUserProfilePath());
  DCHECK(profile);
  Browser* browser =
      chrome::FindAnyBrowser(profile, /*match_original_profiles=*/true);
  if (!browser) {
    // TODO(https://crbug.com/1300109): Create a browser to run `callback`.
    DLOG(WARNING)
        << "No browser found when finishing Lacros FRE. Expected to find "
        << "one for the primary profile.";
    return;
  }

  std::move(callback).Run(browser);
}

}  // namespace

FirstRunFlowControllerLacros::FirstRunFlowControllerLacros(
    ProfilePickerWebContentsHost* host,
    Profile* profile,
    ProfilePicker::DebugFirstRunExitedCallback first_run_exited_callback)
    : ProfileManagementFlowController(host, Step::kPostSignInFlow),
      first_run_exited_callback_(std::move(first_run_exited_callback)) {
  DCHECK(first_run_exited_callback_);

  auto finish_and_continue_in_browser_callback =
      base::BindOnce(&FirstRunFlowControllerLacros::ExitFlowAndRun,
                     // Unretained ok: the callback is passed to a step that
                     // the `this` will own and outlive.
                     base::Unretained(this));

  auto signed_in_flow = std::make_unique<LacrosFirstRunSignedInFlowController>(
      host, profile,
      content::WebContents::Create(content::WebContents::CreateParams(profile)),
      std::move(finish_and_continue_in_browser_callback));
  signed_in_flow_ = signed_in_flow->GetWeakPtr();

  RegisterStep(initial_step(),
               ProfileManagementStepController::CreateForPostSignInFlow(
                   host, std::move(signed_in_flow)));
}

FirstRunFlowControllerLacros::~FirstRunFlowControllerLacros() {
  // Call the callback if not called yet. This can happen in case of early
  // exits for example, the original intent callback just gets dropped. See
  // https://crbug.com/1307754.
  if (first_run_exited_callback_ && signed_in_flow_) {
    std::move(first_run_exited_callback_)
        .Run(signed_in_flow_->sync_confirmation_seen()
                 ? ProfilePicker::FirstRunExitStatus::kQuitAtEnd
                 : ProfilePicker::FirstRunExitStatus::kQuitEarly,
             ProfilePicker::FirstRunExitSource::kControllerDestructor,
             // Since the flow is exited already, we don't have anything to
             // close or finish setting up, and the callback won't be executed
             // anyway.
             /*maybe_callback=*/base::OnceClosure());
  }
}

void FirstRunFlowControllerLacros::ExitFlowAndRun(
    ProfilePicker::BrowserOpenedCallback callback) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile = profile_manager->GetProfileByPath(
      profile_manager->GetPrimaryUserProfilePath());
  DCHECK(profile);

  std::move(first_run_exited_callback_)
      .Run(ProfilePicker::FirstRunExitStatus::kCompleted,
           ProfilePicker::FirstRunExitSource::kFlowFinished,
           base::BindOnce(&HideProfilePickerAndRun, std::move(callback)));
}
