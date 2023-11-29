// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_CAMERA_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_CAMERA_COORDINATOR_H_

#include <optional>
#include <string>
#include <vector>

#include "chrome/browser/ui/views/media_preview/camera_preview/camera_mediator.h"
#include "chrome/browser/ui/views/media_preview/camera_preview/camera_selector_combobox_model.h"
#include "chrome/browser/ui/views/media_preview/camera_preview/camera_view_controller.h"
#include "chrome/browser/ui/views/media_preview/camera_preview/video_stream_coordinator.h"
#include "ui/views/view_tracker.h"

// Acts as a middle man between the ViewController and the Mediator.
// Maintains the lifetime of its views.
class CameraCoordinator {
 public:
  CameraCoordinator(views::View& parent_view, bool needs_borders);
  CameraCoordinator(const CameraCoordinator&) = delete;
  CameraCoordinator& operator=(const CameraCoordinator&) = delete;
  ~CameraCoordinator();

  // Invoked from the ViewController when a combobox selection has been made.
  void OnVideoSourceChanged(std::optional<size_t> selected_index);

  const CameraSelectorComboboxModel& GetComboboxModelForTest() const {
    return combobox_model_;
  }

 private:
  // `device_infos` is  a list of connected devices. When a new device
  // gets connected or a device gets disconnected, this function is called to
  // with the new list.
  void OnVideoSourceInfosReceived(
      const std::vector<media::VideoCaptureDeviceInfo>& device_infos);

  void ResetViewController();

  CameraMediator camera_mediator_;
  views::ViewTracker camera_view_tracker_;
  CameraSelectorComboboxModel combobox_model_;
  std::string active_device_id_;
  std::optional<CameraViewController> camera_view_controller_;
  std::optional<VideoStreamCoordinator> video_stream_coordinator_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_CAMERA_COORDINATOR_H_
