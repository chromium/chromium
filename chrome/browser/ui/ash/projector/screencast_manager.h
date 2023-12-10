// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_PROJECTOR_SCREENCAST_MANAGER_H_
#define CHROME_BROWSER_UI_ASH_PROJECTOR_SCREENCAST_MANAGER_H_

#include <optional>
#include <string>

#include "ash/webui/projector_app/projector_app_client.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace file_manager {
class ScopedSuppressDriveNotificationsForPath;
}

namespace ash {

// Class to get and modify screencast data through IO and DriveFS.
class ScreencastManager {
 public:
  ScreencastManager();
  ScreencastManager(const ScreencastManager&) = delete;
  ScreencastManager& operator=(const ScreencastManager&) = delete;
  ~ScreencastManager();

  // Launches the given DriveFS video file with `video_file_id` into the
  // Projector app. The `resource_key` is an additional security token needed to
  // gain access to link-shared files. Since the `resource_key` is currently
  // only used by Googlers, the `resource_key` might be empty.
  void GetVideo(const std::string& video_file_id,
                const std::optional<std::string>& resource_key,
                ProjectorAppClient::OnGetVideoCallback callback) const;

  // Resets `suppress_drive_notifications_for_path_`. Called when the app UI is
  // destroyed or the path doesn't pass the video duration check.
  void ResetScopeSuppressDriveNotifications();

 private:
  // If `paths` has no error, suppresses the notification for the give path and
  // triggers video duration verification on `video_metadata_task_runner_`.
  void OnVideoFilePathLocated(
      const std::string& video_id,
      ProjectorAppClient::OnGetVideoCallback callback,
      std::optional<std::vector<drivefs::mojom::FilePathOrErrorPtr>> paths);

  // The task runner to get video metadata.
  scoped_refptr<base::SequencedTaskRunner> video_metadata_task_runner_;

  // The video path to ignore for for Drive system notification when the
  // Projector app is active.
  std::unique_ptr<file_manager::ScopedSuppressDriveNotificationsForPath>
      suppress_drive_notifications_for_path_;

  base::WeakPtrFactory<ScreencastManager> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_PROJECTOR_SCREENCAST_MANAGER_H_
