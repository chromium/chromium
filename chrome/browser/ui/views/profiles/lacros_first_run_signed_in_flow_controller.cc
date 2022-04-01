// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/lacros_first_run_signed_in_flow_controller.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/notreached.h"
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
    if (callback_)
      std::move(callback_).Run();
  }

 private:
  base::OnceClosure callback_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
};

// Helper to run `callback`, after hiding the profile picker.
// Unlike other signed in flow controllers, this one completes by staying in
// the current profile instead of switching to a new one, which normally would
// have handled hiding the picker.
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

LacrosFirstRunSignedInFlowController::LacrosFirstRunSignedInFlowController(
    ProfilePickerWebContentsHost* host,
    Profile* profile,
    std::unique_ptr<content::WebContents> contents,
    ProfilePicker::FirstRunExitedCallback first_run_exited_callback)
    : ProfilePickerSignedInFlowController(host,
                                          profile,
                                          std::move(contents),
                                          absl::optional<SkColor>()),
      first_run_exited_callback_(std::move(first_run_exited_callback)) {}

LacrosFirstRunSignedInFlowController::~LacrosFirstRunSignedInFlowController() {
  // Call the callback if not called yet. This can happen in case of early exits
  // for example, the registered callback just gets dropped. See
  // https://crbug.com/1307754.
  if (first_run_exited_callback_) {
    std::move(first_run_exited_callback_)
        .Run(sync_confirmation_seen_
                 ? ProfilePicker::FirstRunExitStatus::kQuitAtEnd
                 : ProfilePicker::FirstRunExitStatus::kQuitEarly,
             base::BindOnce(&HideProfilePickerAndRun,
                            ProfilePicker::BrowserOpenedCallback()));
  }
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

void LacrosFirstRunSignedInFlowController::FinishAndOpenBrowser(
    ProfilePicker::BrowserOpenedCallback callback) {
  if (!first_run_exited_callback_)
    return;

  std::move(first_run_exited_callback_)
      .Run(ProfilePicker::FirstRunExitStatus::kCompleted,
           base::BindOnce(&HideProfilePickerAndRun, std::move(callback)));
}

void LacrosFirstRunSignedInFlowController::SwitchToSyncConfirmation() {
  sync_confirmation_seen_ = true;

  ProfilePickerSignedInFlowController::SwitchToSyncConfirmation();
}
