// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/lacros_first_run_signed_in_flow_controller.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "chrome/browser/signin/identity_manager_factory.h"
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
    if (callback_)
      std::move(callback_).Run();
  }

 private:
  base::OnceClosure callback_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
};

// Helper to run `callback` with `browser`, after hiding the profile picker.
// Unlike other signed in flow controllers, this one completes by staying in
// the current profile instead of switching to a new one, which normally would
// have handled hiding the picker.
void HideProfilePickerAndRun(ProfilePicker::BrowserOpenedCallback callback,
                             Browser* browser) {
  ProfilePicker::Hide();
  if (callback)
    std::move(callback).Run(browser);
}

}  // namespace

LacrosFirstRunSignedInFlowController::LacrosFirstRunSignedInFlowController(
    ProfilePickerWebContentsHost* host,
    Profile* profile,
    std::unique_ptr<content::WebContents> contents,
    absl::optional<SkColor> profile_color,
    OnboardingFinishedCallback onboarding_finished_callback)
    : ProfilePickerSignedInFlowController(host,
                                          profile,
                                          std::move(contents),
                                          profile_color),
      onboarding_finished_callback_(std::move(onboarding_finished_callback)) {}

LacrosFirstRunSignedInFlowController::~LacrosFirstRunSignedInFlowController() {
  // Call the callback if not called yet (unless the flow has been canceled).
  if (onboarding_finished_callback_)
    std::move(onboarding_finished_callback_)
        .Run(base::BindOnce(&HideProfilePickerAndRun,
                            ProfilePicker::BrowserOpenedCallback()));
}

void LacrosFirstRunSignedInFlowController::Init() {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());

  if (can_retry_init_observer_)
    can_retry_init_observer_.reset();

  if (!identity_manager->AreRefreshTokensLoaded()) {
    // We can't proceed with the init yet, as the tokens will be needed to
    // obtain extended account info and turn on sync. Register this method to be
    // called again when they become available.
    can_retry_init_observer_ = std::make_unique<OnRefreshTokensLoadedObserver>(
        identity_manager,
        base::BindOnce(&LacrosFirstRunSignedInFlowController::Init,
                       base::Unretained(this)));
    return;
  }

  ProfilePickerSignedInFlowController::Init();
}

void LacrosFirstRunSignedInFlowController::Cancel() {
  // If the flow gets canceled in the first (welcome) screen, it is not
  // considered as finished (and thus the callback should not get called).
  onboarding_finished_callback_.Reset();
}

void LacrosFirstRunSignedInFlowController::FinishAndOpenBrowser(
    ProfilePicker::BrowserOpenedCallback callback) {
  // Do nothing if this has already been called. Note that this can get called
  // first time from a special case handling (such as the Settings link) and
  // than second time when the `TurnSyncOnHelper` finishes.
  if (!onboarding_finished_callback_)
    return;

  // TODO(crbug.com/1300109): Rename the profile here.

  std::move(onboarding_finished_callback_)
      .Run(base::BindOnce(&HideProfilePickerAndRun, std::move(callback)));
}
