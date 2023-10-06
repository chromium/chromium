// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_PROJECTOR_PENDING_SCREENCAST_MANAGER_H_
#define CHROME_BROWSER_UI_ASH_PROJECTOR_PENDING_SCREENCAST_MANAGER_H_

#include <map>
#include <memory>

#include "ash/webui/projector_app/projector_app_client.h"
#include "ash/webui/projector_app/projector_xhr_sender.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/extensions/file_manager/scoped_suppress_drive_notifications_for_path.h"
#include "chrome/browser/ui/ash/projector/projector_drivefs_provider.h"
#include "chromeos/ash/components/drivefs/drivefs_host.h"

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
    base::RepeatingCallback<void(const ash::PendingScreencastContainerSet&)>;

// A class that handles pending screencast events.
class PendingScreencastManager : drivefs::DriveFsHost::Observer {
 public:
  explicit PendingScreencastManager(
      PendingScreencastChangeCallback pending_screencast_change_callback);
  PendingScreencastManager(const PendingScreencastManager&) = delete;
  PendingScreencastManager& operator=(const PendingScreencastManager&) = delete;
  ~PendingScreencastManager() override;

  // DriveFsHost::Observer implementation.
  using drivefs::DriveFsHost::Observer::GetHost;
  void OnUnmounted() override;
  void OnSyncingStatusUpdate(
      const drivefs::mojom::SyncingStatus& status) override;
  void OnError(const drivefs::mojom::DriveError& error) override;

  // Returns a list of pending screencast from `pending_screencast_cache_`.
  const ash::PendingScreencastContainerSet& GetPendingScreencasts() const;

  // Maybe observe the current active profile.
  void MaybeSwitchDriveFsObservation();

  // Adds `screencast_paths` to `paths_notifications_suppressors_` and
  // suppresses notification for these paths if `suppress` is true. Removes
  // `screencast_paths` from `paths_notifications_suppressors_` when
  // `suppress` is false.
  void ToggleFileSyncingNotificationForPaths(
      const std::vector<base::FilePath>& screencast_paths,
      bool suppress);

  // Resets (`is_active` is false) or creates (`is_active` is true) values for
  // all keys stored in `paths_notifications_suppressors_`.
  void OnAppActiveStatusChanged(bool is_active);

  // Test only:
  base::TimeTicks last_pending_screencast_change_tick() const {
    return last_pending_screencast_change_tick_;
  }
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner() {
    return blocking_task_runner_;
  }

  using OnGetFileIdCallback =
      base::OnceCallback<void(const base::FilePath& local_file_path,
                              const std::string& file_id)>;
  void SetOnGetFileIdCallbackForTest(OnGetFileIdCallback callback);
  using OnGetRequestBodyCallback =
      base::OnceCallback<void(const std::string& file_id,
                              const std::string& request_body)>;
  void SetOnGetRequestBodyCallbackForTest(OnGetRequestBodyCallback callback);
  void SetProjectorXhrSenderForTest(
      std::unique_ptr<ash::ProjectorXhrSender> xhr_sender);

 private:
  // Updates `pending_screencast_cache_` and notifies pending screencast change.
  void OnProcessAndGenerateNewScreencastsFinished(
      const base::TimeTicks task_start_tick,
      const ash::PendingScreencastContainerSet& screencasts);

  // Called when the `event_file` is synced to Drive. Removed completedly synced
  // files from `error_syncing_files_` and `syncing_metadata_files_` cached. If
  // it is a screencast metadata file, post task to update indexable text.
  void OnFileSyncedCompletely(const base::FilePath& event_file);

  void OnGetFileId(const base::FilePath& local_file_path,
                   const std::string& file_id);

  // Sends a patch request to patch file metadata. `file_id` is the Drive server
  // side file id.
  void SendDrivePatchRequest(const std::string& file_id,
                             const std::string& request_body);

  // TODO(b/221902328): Fix the case that user might delete files through file
  // app.

  // A set that caches current pending screencast.
  ash::PendingScreencastContainerSet pending_screencast_cache_;

  // A set of files failed to upload to Drive.
  std::set<base::FilePath> error_syncing_files_;

  // A set of syncing screencast metadata files, which have ".projector"
  // extension. This set is used to track which metadata files are being
  // uploaded so we only update the indexable text once. File is removed from
  // the set after updating indexable text completed.
  std::set<base::FilePath> syncing_metadata_files_;

  // A callback to notify pending screencast status change.
  PendingScreencastChangeCallback pending_screencast_change_callback_;

  // A blocking task runner for file IO operations.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  // The time tick when last `pending_screencast_change_callback_` was called.
  // Could be null if last `pending_screencast_change_callback_` was called with
  // empty screencasts set or no `pending_screencast_change_callback_` invoked
  // in the current ChromeOS session.
  base::TimeTicks last_pending_screencast_change_tick_;

  // Not available if user never uploads a screencast during current ChromeOS
  // session.
  std::unique_ptr<ash::ProjectorXhrSender> xhr_sender_;

  // Updates indexable text containing a lot of async steps. These callbacks are
  // used in tests to verify the task quit correctly while error happens.
  OnGetRequestBodyCallback on_get_request_body_;
  OnGetFileIdCallback on_get_file_id_callback_;

  ProjectorDriveFsProvider drive_helper_;

  // A map to store `file_manager::ScopedSuppressDriveNotificationsForPath`. The
  // entries get created/destroyed on calling
  // `ToggleFileSyncingNotificationForPaths`, or when files whose paths are
  // stored in this map are uploaded completely. All unique pointers get reset
  // on app UI destroyed and re-created on app UI active.
  std::map<
      base::FilePath,
      std::unique_ptr<file_manager::ScopedSuppressDriveNotificationsForPath>>
      paths_notifications_suppressors_;

  base::WeakPtrFactory<PendingScreencastManager> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_PROJECTOR_PENDING_SCREENCAST_MANAGER_H_
