// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_CAMERA_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_CAMERA_COORDINATOR_H_

#include <optional>
#include <vector>

#include "chrome/browser/ui/views/media_preview/camera_preview/camera_mediator.h"
#include "chrome/browser/ui/views/media_preview/camera_preview/camera_view_controller.h"
#include "ui/views/view_tracker.h"

// Acts as a middle man between the ViewController and the Mediator.
// Maintains the lifetime of its views.
class CameraCoordinator {
 public:
  CameraCoordinator(views::View& parent_view, bool needs_borders);
  CameraCoordinator(const CameraCoordinator&) = delete;
  CameraCoordinator& operator=(const CameraCoordinator&) = delete;
  ~CameraCoordinator();

 private:
  friend class CameraCoordinatorTest;

  // `device_infos` is  a list of connected devices. When a new device
  // gets connected or a device gets disconnected, this function is called to
  // with the new list.
  void OnVideoSourceInfosReceived(
      const std::vector<media::VideoCaptureDeviceInfo>& device_infos);

  // Invoked from the ViewController when a combobox selection has been made.
  void OnVideoSourceChanged(const VideoSourceInfo& video_source_info);

  void ResetViewController();

  CameraMediator camera_mediator_;
  views::ViewTracker camera_view_tracker_;
  CameraSelectorComboboxModel combobox_model_;
  std::optional<CameraViewController> camera_view_controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_CAMERA_COORDINATOR_H_
