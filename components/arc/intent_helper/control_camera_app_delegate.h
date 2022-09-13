// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INTENT_HELPER_CONTROL_CAMERA_APP_DELEGATE_H_
#define COMPONENTS_ARC_INTENT_HELPER_CONTROL_CAMERA_APP_DELEGATE_H_

#include <string>

namespace arc {

class ControlCameraAppDelegate {
 public:
  virtual ~ControlCameraAppDelegate() = default;

  // Launches the camera app from Android camera intent with the intent
  // information as url |queries|. |task_id| represents the id of the caller
  // task in ARC.
  virtual void LaunchCameraApp(const std::string& queries, int32_t task_id) = 0;

  // Closes the camera app.
  virtual void CloseCameraApp() = 0;

  // Checks if Chrome Camera App is enabled.
  virtual bool IsCameraAppEnabled() = 0;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_INTENT_HELPER_CONTROL_CAMERA_APP_DELEGATE_H_
