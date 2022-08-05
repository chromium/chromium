// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_PROJECTOR_SCREENCAST_MANAGER_H_
#define CHROME_BROWSER_UI_ASH_PROJECTOR_SCREENCAST_MANAGER_H_

namespace ash {

// Class to get and modify screencast data through IO and DriveFS.
class ScreencastManager {
 public:
  ScreencastManager();
  ScreencastManager(const ScreencastManager&) = delete;
  ScreencastManager& operator=(const ScreencastManager&) = delete;
  ~ScreencastManager();
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_PROJECTOR_SCREENCAST_MANAGER_H_
