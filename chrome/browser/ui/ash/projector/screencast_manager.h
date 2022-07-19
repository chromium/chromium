// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_PROJECTOR_SCREENCAST_MANAGER_H_
#define CHROME_BROWSER_UI_ASH_PROJECTOR_SCREENCAST_MANAGER_H_

#include <string>

#include "ash/webui/projector_app/projector_app_client.h"

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

  // TODO(b/236857019):
  // SearchScreencastFilesByParentId(): Call rest API to populate screencast
  // metadata file id and video file id.
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_PROJECTOR_SCREENCAST_MANAGER_H_
