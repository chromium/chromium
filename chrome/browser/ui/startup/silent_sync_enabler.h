
// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_SILENT_SYNC_ENABLER_H_
#define CHROME_BROWSER_UI_STARTUP_SILENT_SYNC_ENABLER_H_

#include "base/functional/callback_forward.h"
#include "base/scoped_observation.h"
#include "components/signin/public/identity_manager/identity_manager.h"

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
#error This is only supported on lacros.
#endif

class Profile;

// Allows turning on Sync silently by running through the regular flow without
// showing any UI.
// Use it by calling `SilentSyncEnabler::StartAttempt()`, keep the returned
// instance around to make sure multiple attempts are not happening at the same
// time.
// Intended to be used when migrating previously syncing profiles to Lacros.
class SilentSyncEnabler : public signin::IdentityManager::Observer {
 public:
  explicit SilentSyncEnabler(Profile* profile);
  ~SilentSyncEnabler() override;

  SilentSyncEnabler(const SilentSyncEnabler&) = delete;
  SilentSyncEnabler& operator=(const SilentSyncEnabler&) = delete;

  // Attempts to turn Sync on with `profile_`'s primary account and runs
  // `callback` when the attempts completes, whether it succeeded or not.
  // Notes:
  // - Deleting the returned instance will not cancel the attempt and
  //   `callback` can potentially be executed after this instance has been
  //   deleted. It is the caller's responsibility to ensure that it is safe to
  //   do so.
  // - If this is called while another attempt is happening on the same profile,
  //   the other attempt will be cancelled (per TurnSyncOnHelper's behaviour,
  //   only one can be attached to a profile at a time).
  void StartAttempt(base::OnceClosure callback);

 private:
  void TryEnableSyncSilentlyWithToken();

  // signin::IdentityManager::Observer:
  void OnRefreshTokensLoaded() override;

  raw_ptr<Profile> profile_;
  base::OnceClosure callback_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      scoped_observation_{this};
};

#endif  // CHROME_BROWSER_UI_STARTUP_SILENT_SYNC_ENABLER_H_
