// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/lacros_first_run_signed_in_flow_controller.h"

#include "base/logging.h"
#include "base/scoped_observation.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_finder.h"
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

}  // namespace

LacrosFirstRunSignedInFlowController::LacrosFirstRunSignedInFlowController(
    ProfilePickerWebContentsHost* host,
    Profile* profile,
    std::unique_ptr<content::WebContents> contents,
    base::OnceCallback<void(ProfilePicker::BrowserOpenedCallback
                                maybe_callback)> flow_completed_callback)
    : ProfilePickerSignedInFlowController(host,
                                          profile,
                                          std::move(contents),
                                          absl::optional<SkColor>()),
      flow_completed_callback_(std::move(flow_completed_callback)) {}

LacrosFirstRunSignedInFlowController::~LacrosFirstRunSignedInFlowController() =
    default;

void LacrosFirstRunSignedInFlowController::Init() {
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
    // obtain extended account info and turn on sync. Register this method to be
    // called again when they become available.
    can_retry_init_observer_ = std::make_unique<OnRefreshTokensLoadedObserver>(
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

void LacrosFirstRunSignedInFlowController::FinishAndOpenBrowser(
    ProfilePicker::BrowserOpenedCallback callback) {
  if (flow_completed_callback_)
    std::move(flow_completed_callback_).Run(std::move(callback));
}

void LacrosFirstRunSignedInFlowController::SwitchToSyncConfirmation() {
  sync_confirmation_seen_ = true;

  ProfilePickerSignedInFlowController::SwitchToSyncConfirmation();
}

void LacrosFirstRunSignedInFlowController::PreShowScreenForDebug() {
  LOG(WARNING) << "Calling ShowScreen()";
}
