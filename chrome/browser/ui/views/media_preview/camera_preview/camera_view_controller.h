// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_CAMERA_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_CAMERA_VIEW_CONTROLLER_H_

#include <stddef.h>

#include <memory>

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/views/media_preview/media_view_controller_base.h"

namespace media {
struct VideoCaptureDeviceInfo;
}  // namespace media
class MediaView;
namespace ui {
class SimpleComboboxModel;
}  // namespace ui

// The MediaViewController for the camera view. It sets up the camera
// view, and updates it based on the data it receives from the Coordinator. Also
// it notifies the coordinator of changes resulting from events on the view.
class CameraViewController {
 public:
  CameraViewController(MediaView& base_view,
                       bool needs_borders,
                       ui::SimpleComboboxModel& combobox_model,
                       bool allow_device_selection,
                       MediaViewControllerBase::SourceChangeCallback callback,
                       media_preview_metrics::Context metrics_context);
  CameraViewController(const CameraViewController&) = delete;
  CameraViewController& operator=(const CameraViewController&) = delete;
  ~CameraViewController();

  // Returns the immediate parent view of the live camera feed.
  MediaView& GetLiveFeedContainer();

  // `video_source_infos` is  a list of connected devices. When a new device
  // gets connected or a device gets disconnected, this function is called to
  // update the list of devices in the combobox.
  void UpdateVideoSourceInfos(
      const std::vector<media::VideoCaptureDeviceInfo>& video_source_infos);

 private:
  const raw_ref<ui::SimpleComboboxModel> combobox_model_;
  std::unique_ptr<MediaViewControllerBase> base_controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_CAMERA_VIEW_CONTROLLER_H_
