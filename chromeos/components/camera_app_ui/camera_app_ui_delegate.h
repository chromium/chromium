// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_CAMERA_APP_UI_CAMERA_APP_UI_DELEGATE_H_
#define CHROMEOS_COMPONENTS_CAMERA_APP_UI_CAMERA_APP_UI_DELEGATE_H_

#include <string>

namespace content {
class WebUIDataSource;
}

// A delegate which exposes browser functionality from //chrome to the camera
// app ui page handler.
class CameraAppUIDelegate {
 public:
  virtual ~CameraAppUIDelegate() = default;

  // Sets Downloads folder as launch directory by File Handling API so that we
  // can get the handle on the app side.
  virtual void SetLaunchDirectory() = 0;

  // Takes a WebUIDataSource, and adds load time data into it.
  virtual void PopulateLoadTimeData(content::WebUIDataSource* source) = 0;

  // TODO(crbug.com/1113567): Remove this method once we migrate to use UMA to
  // collect metrics. Checks if the logging consent option is enabled.
  virtual bool IsMetricsAndCrashReportingEnabled() = 0;

  // Opens the file in Downloads folder by its |name| in gallery.
  virtual void OpenFileInGallery(const std::string& name) = 0;

  // Opens the native chrome feedback dialog scoped to chrome://camera-app and
  // show |placeholder| in the description field.
  virtual void OpenFeedbackDialog(const std::string& placeholder) = 0;
};

#endif  // CHROMEOS_COMPONENTS_CAMERA_APP_UI_CAMERA_APP_UI_DELEGATE_H_
