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
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace drivefs {
namespace mojom {
class SyncingStatus;
class DriveError;
}  // namespace mojom
}  // namespace drivefs

// A callback to notify the change of pending screencasts to
// ProjectorAppClient::Observer. The argument is the set of pending screencasts
// owned by PendingSreencastManager.
using PendingScreencastChangeCallback =
    base::RepeatingCallback<void(const ash::PendingScreencastSet&)>;

// A class that handles pending screencast events.
class PendingSreencastManager : public session_manager::SessionManagerObserver,
                                public drivefs::DriveFsHostObserver {
 public:
  explicit PendingSreencastManager(
      PendingScreencastChangeCallback pending_screencast_change_callback);
  PendingSreencastManager(const PendingSreencastManager&) = delete;
  PendingSreencastManager& operator=(const PendingSreencastManager&) = delete;
  ~PendingSreencastManager() override;

  // drivefs::DriveFsHostObserver:
  void OnUnmounted() override;
  void OnSyncingStatusUpdate(
      const drivefs::mojom::SyncingStatus& status) override;
  void OnError(const drivefs::mojom::DriveError& error) override;

  // session_manager::SessionManagerObserver:
  void OnUserSessionStarted(bool is_primary_user) override;

  // Returns a list of pending screencast from `pending_screencast_cache_`.
  const ash::PendingScreencastSet& GetPendingScreencasts() const;

 private:
  // Updates `pending_screencast_cache_` and notifies pending screencast change.
  void OnProcessAndGenerateNewScreencastsFinished(
      const ash::PendingScreencastSet& screencasts);

  // A set that caches current pending screencast.
  ash::PendingScreencastSet pending_screencast_cache_;

  // A callback to notify pending screencast status change.
  PendingScreencastChangeCallback pending_screencast_change_callback_;

  // A blocking task runner for file IO operations.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  base::WeakPtrFactory<PendingSreencastManager> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_PROJECTOR_PENDING_SCREENCAST_MANAGER_H_
