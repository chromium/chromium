// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/first_run_flow_controller_lacros.h"

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_utils.h"
#include "chrome/browser/ui/views/profiles/profile_picker_signed_in_flow_controller.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace {
// Registers a new `Observer` that will invoke `callback_` when `manager`
// notifies it via `OnRefreshTokensLoaded()`.
class OnRefreshTokensLoadedObserver : public signin::IdentityManager::Observer {
 public:
  OnRefreshTokensLoadedObserver(signin::IdentityManager* manager,
                                base::OnceClosure callback)
      : callback_(std::move(callback)) {
    DCHECK(callback_);
    identity_manager_observation_.Observe(manager);
  }

  // signin::IdentityManager::Observer
  void OnRefreshTokensLoaded() override {
    identity_manager_observation_.Reset();

    if (callback_)
      std::move(callback_).Run();
  }

 private:
  base::OnceClosure callback_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
};

class LacrosFirstRunSignedInFlowController
    : public ProfilePickerSignedInFlowController {
 public:
  // `finish_flow_callback` will be called when the user completes the FRE, but
  // might not be executed, for example if this object is destroyed before the
  // flow is completed.
  LacrosFirstRunSignedInFlowController(
      ProfilePickerWebContentsHost* host,
      Profile* profile,
      std::unique_ptr<content::WebContents> contents,
      base::OnceClosure sync_confirmation_seen_callback,
      FinishFlowCallback finish_flow_callback)
      : ProfilePickerSignedInFlowController(host,
                                            profile,
                                            std::move(contents),
                                            absl::optional<SkColor>()),
        sync_confirmation_seen_callback_(
            std::move(sync_confirmation_seen_callback)),
        finish_flow_callback_(std::move(finish_flow_callback)) {}

  ~LacrosFirstRunSignedInFlowController() override = default;

  // ProfilePickerSignedInFlowController:
  void Init() override {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile());

    if (can_retry_init_observer_)
      can_retry_init_observer_.reset();

    LOG(WARNING) << "Init running "
                 << (identity_manager->AreRefreshTokensLoaded() ? "with"
                                                                : "without")
                 << " refresh tokens.";

    if (!identity_manager->AreRefreshTokensLoaded()) {
      // We can't proceed with the init yet, as the tokens will be needed to
      // obtain extended account info and turn on sync. Register this method to
      // be called again when they become available.
      can_retry_init_observer_ =
          std::make_unique<OnRefreshTokensLoadedObserver>(
              identity_manager,
              base::BindOnce(
                  &LacrosFirstRunSignedInFlowController::Init,
                  // Unretained ok: `this` owns the observer and outlives it.
                  base::Unretained(this)));
      return;
    }

    ProfilePickerSignedInFlowController::Init();

    LOG(WARNING)
        << "Init completed and initiative handed off to TurnSyncOnHelper.";
  }

  void FinishAndOpenBrowser(PostHostClearedCallback callback) override {
    if (finish_flow_callback_.value())
      std::move(finish_flow_callback_.value()).Run(std::move(callback));
  }

  void SwitchToSyncConfirmation() override {
    DCHECK(sync_confirmation_seen_callback_);  // Should be called only once.
    std::move(sync_confirmation_seen_callback_).Run();

    ProfilePickerSignedInFlowController::SwitchToSyncConfirmation();
  }

 protected:
  void PreShowScreenForDebug() override {
    LOG(WARNING) << "Calling ShowScreen()";
  }

 private:
  // Callback that gets called when the user gets to the last step of the FRE.
  base::OnceClosure sync_confirmation_seen_callback_;

  // Callback that will be called when the user completes all the steps in the
  // flow, to finalize and close it.
  FinishFlowCallback finish_flow_callback_;

  std::unique_ptr<signin::IdentityManager::Observer> can_retry_init_observer_;
};

}  // namespace

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

  auto mark_sync_confirmation_seen_callback =
      base::BindOnce(&FirstRunFlowControllerLacros::MarkSyncConfirmationSeen,
                     // Unretained ok: the callback is passed to a step that
                     // the `this` will own and outlive.
                     base::Unretained(this));
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
      std::move(mark_sync_confirmation_seen_callback),
      std::move(finish_flow_callback));

  RegisterStep(initial_step(),
               ProfileManagementStepController::CreateForPostSignInFlow(
                   host, std::move(signed_in_flow)));
}

FirstRunFlowControllerLacros::~FirstRunFlowControllerLacros() {
  // Call the callback if not called yet. This happens when the user exits the
  // flow by closing the window, or for intent overrides.
  if (first_run_exited_callback_) {
    std::move(first_run_exited_callback_)
        .Run(sync_confirmation_seen_
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

void FirstRunFlowControllerLacros::MarkSyncConfirmationSeen() {
  sync_confirmation_seen_ = true;
}
