// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/first_run_flow_controller_lacros.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/profiles/lacros_first_run_signed_in_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"

FirstRunFlowControllerLacros::FirstRunFlowControllerLacros(
    ProfilePickerWebContentsHost* host,
    ClearHostClosure clear_host_callback,
    Profile* profile,
    ProfilePicker::DebugFirstRunExitedCallback first_run_exited_callback)
    : ProfileManagementFlowController(host,
                                      std::move(clear_host_callback),
                                      Step::kPostSignInFlow),
      first_run_exited_callback_(std::move(first_run_exited_callback)) {
  DCHECK(first_run_exited_callback_);

  auto finish_flow_callback = FinishFlowCallback(
      base::BindOnce(&FirstRunFlowControllerLacros::ExitFlowAndRun,
                     // Unretained ok: the callback is passed to a step that
                     // the `this` will own and outlive.
                     base::Unretained(this),
                     // Unretained ok: `signed_in_flow` will register a profile
                     // keep alive.
                     base::Unretained(profile)));

  auto signed_in_flow = std::make_unique<LacrosFirstRunSignedInFlowController>(
      host, profile,
      content::WebContents::Create(content::WebContents::CreateParams(profile)),
      std::move(finish_flow_callback));
  signed_in_flow_ = signed_in_flow->GetWeakPtr();

  RegisterStep(initial_step(),
               ProfileManagementStepController::CreateForPostSignInFlow(
                   host, std::move(signed_in_flow)));
}

FirstRunFlowControllerLacros::~FirstRunFlowControllerLacros() {
  // Call the callback if not called yet. This happens when the user exits the
  // flow by closing the window, or for intent overrides.
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
    Profile* profile,
    PostHostClearedCallback callback) {
  // We don't call `FinishFlowAndRunInBrowser()` directly, as
  // `first_run_exited_callback_` should make a browser window available when
  // it runs. If there is no browser, then we will create it as a fallback.
  auto finish_flow_callback =
      base::BindOnce(&FirstRunFlowControllerLacros::FinishFlowAndRunInBrowser,
                     // Unretained ok: the flow will be closed when we run
                     // `finish_flow_callback`, so `this` will still be alive.
                     base::Unretained(this),
                     // Unretained ok: the flow keeps the profile alive and
                     // `first_run_exited_callback_` will open a browser for it.
                     base::Unretained(profile), std::move(callback));

  std::move(first_run_exited_callback_)
      .Run(ProfilePicker::FirstRunExitStatus::kCompleted,
           ProfilePicker::FirstRunExitSource::kFlowFinished,
           std::move(finish_flow_callback));
}
