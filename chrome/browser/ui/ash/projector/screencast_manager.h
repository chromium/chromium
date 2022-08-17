// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_PROJECTOR_SCREENCAST_MANAGER_H_
#define CHROME_BROWSER_UI_ASH_PROJECTOR_SCREENCAST_MANAGER_H_

#include <string>

#include "ash/webui/projector_app/projector_app_client.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

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
                const std::string& resource_key,
                ProjectorAppClient::OnGetVideoCallback callback) const;

 private:
  // The task runner to get video metadata.
  scoped_refptr<base::SequencedTaskRunner> video_metadata_task_runner_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_PROJECTOR_SCREENCAST_MANAGER_H_
