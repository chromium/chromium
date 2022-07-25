// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_PROJECTOR_SCREENCAST_MANAGER_H_
#define CHROME_BROWSER_UI_ASH_PROJECTOR_SCREENCAST_MANAGER_H_

#include <memory>
#include <string>

#include "ash/webui/projector_app/projector_app_client.h"

namespace drive {
class DriveServiceInterface;
}  // namespace drive

namespace ash {

// Class to get and modify screencast data through IO and Drive/DriveFS.
class ScreencastManager {
 public:
  ScreencastManager();
  ScreencastManager(const ScreencastManager&) = delete;
  ScreencastManager& operator=(const ScreencastManager&) = delete;
  ~ScreencastManager();

  // Populates all fields for a screencast except `srcUrl` in `callback` for
  // given `screencast_id`.
  void GetScreencast(const std::string& screencast_id,
                     ProjectorAppClient::OnGetScreencastCallback callback);

  void SetDriveAPIServiceForTest(
      std::unique_ptr<drive::DriveServiceInterface> drive_api_service);

 private:
  // Should not call this in constructor because Drive service for active
  // profile might not be ready.
  void InitDriveAPIService();

  // TODO(b/236857019):
  // GetScreencastMetadata(): Read metadata file into ProjectorScreencast.
  // GetScreencastStatus(): Determine the status of ProjectorScreencast.

  // TODO(b/236857019): Replace the REST API with DriveFs integration service
  // once it supports search file by parent id.
  std::unique_ptr<drive::DriveServiceInterface> drive_api_service_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_PROJECTOR_SCREENCAST_MANAGER_H_
