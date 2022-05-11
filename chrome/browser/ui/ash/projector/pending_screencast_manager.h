// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_PROJECTOR_PENDING_SCREENCAST_MANAGER_H_
#define CHROME_BROWSER_UI_ASH_PROJECTOR_PENDING_SCREENCAST_MANAGER_H_

#include "ash/components/drivefs/drivefs_host.h"
#include "ash/components/drivefs/drivefs_host_observer.h"
#include "ash/webui/projector_app/projector_app_client.h"
#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/user_manager/user_manager.h"

namespace drivefs {
namespace mojom {
class SyncingStatus;
class DriveError;
}  // namespace mojom
}  // namespace drivefs

namespace base {
class FilePath;
}

// A callback to notify the change of pending screencasts to
// ProjectorAppClient::Observer. The argument is the set of pending screencasts
// owned by PendingScreencastManager.
using PendingScreencastChangeCallback =
    base::RepeatingCallback<void(const ash::PendingScreencastSet&)>;

// A class that handles pending screencast events.
class PendingScreencastManager
    : public drivefs::DriveFsHostObserver,
      public user_manager::UserManager::UserSessionStateObserver,
      public session_manager::SessionManagerObserver {
 public:
  explicit PendingScreencastManager(
      PendingScreencastChangeCallback pending_screencast_change_callback);
  PendingScreencastManager(const PendingScreencastManager&) = delete;
  PendingScreencastManager& operator=(const PendingScreencastManager&) = delete;
  ~PendingScreencastManager() override;

  // Test only:
  base::TimeTicks last_pending_screencast_change_tick() const {
    return last_pending_screencast_change_tick_;
  }
  bool IsDriveFsObservationObservingSource(drivefs::DriveFsHost* source) const;

  // drivefs::DriveFsHostObserver:
  void OnUnmounted() override;
  void OnSyncingStatusUpdate(
      const drivefs::mojom::SyncingStatus& status) override;
  void OnError(const drivefs::mojom::DriveError& error) override;

  // Returns a list of pending screencast from `pending_screencast_cache_`.
  const ash::PendingScreencastSet& GetPendingScreencasts() const;

 private:
  // Updates `pending_screencast_cache_` and notifies pending screencast change.
  void OnProcessAndGenerateNewScreencastsFinished(
      const base::TimeTicks task_start_tick,
      const ash::PendingScreencastSet& screencasts);

  // session_manager::SessionManagerObserver:
  void OnUserProfileLoaded(const AccountId& account_id) override;

  // user_manager::UserManager::UserSessionStateObserver:
  void ActiveUserChanged(user_manager::User* active_user) override;

  // Maybe reset `drivefs_observation_` and observe the current active profile.
  void MaybeSwitchDriveFsObservation();

  // TODO(b/221902328): Fix the case that user might delete files through file
  // app.

  // A set that caches current pending screencast.
  ash::PendingScreencastSet pending_screencast_cache_;

  // A set of files failed to upload to Drive.
  std::set<base::FilePath> error_syncing_files_;

  // A callback to notify pending screencast status change.
  PendingScreencastChangeCallback pending_screencast_change_callback_;

  // A blocking task runner for file IO operations.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  base::ScopedObservation<drivefs::DriveFsHost, drivefs::DriveFsHostObserver>
      drivefs_observation_{this};
  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_observation_{this};

  base::ScopedObservation<
      user_manager::UserManager,
      user_manager::UserManager::UserSessionStateObserver,
      &user_manager::UserManager::AddSessionStateObserver,
      &user_manager::UserManager::RemoveSessionStateObserver>
      session_state_observation_{this};

  // The time tick when last `pending_screencast_change_callback_` was called.
  // Could be null if last `pending_screencast_change_callback_` was called with
  // empty screencasts set or no `pending_screencast_change_callback_` invoked
  // in the current ChromeOS session.
  base::TimeTicks last_pending_screencast_change_tick_;

  base::WeakPtrFactory<PendingScreencastManager> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_PROJECTOR_PENDING_SCREENCAST_MANAGER_H_
