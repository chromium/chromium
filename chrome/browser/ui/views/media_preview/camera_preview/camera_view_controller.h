// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_CAMERA_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_CAMERA_VIEW_CONTROLLER_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/views/media_preview/media_view_controller_base.h"
#include "media/capture/video_capture_types.h"
#include "ui/base/models/combobox_model.h"

class MediaView;

// A struct to only store relevant info needed about each video source device.
struct VideoSourceInfo {
  VideoSourceInfo(const std::string& id,
                  const std::u16string& name_and_model,
                  const media::VideoCaptureFormats& supported_formats);
  VideoSourceInfo(const VideoSourceInfo& other);
  VideoSourceInfo& operator=(const VideoSourceInfo& other) = delete;
  VideoSourceInfo(VideoSourceInfo&& other);
  VideoSourceInfo& operator=(VideoSourceInfo&& other) = delete;
  ~VideoSourceInfo();

  bool operator==(const VideoSourceInfo& other) const;

  const std::string id;
  const std::u16string name_and_model;
  const media::VideoCaptureFormats supported_formats;
};

class CameraSelectorComboboxModel : public ui::ComboboxModel {
 public:
  CameraSelectorComboboxModel();
  CameraSelectorComboboxModel(const CameraSelectorComboboxModel&) = delete;
  CameraSelectorComboboxModel& operator=(const CameraSelectorComboboxModel&) =
      delete;
  ~CameraSelectorComboboxModel() override;

  const VideoSourceInfo& GetDeviceInfoAt(size_t index) const;

  void UpdateDeviceList(std::vector<VideoSourceInfo> video_source_infos);

  // ui::ComboboxModel:
  size_t GetItemCount() const override;
  std::u16string GetItemAt(size_t index) const override;

 private:
  std::vector<VideoSourceInfo> video_source_infos_;
};

// The MediaViewController for the camera view. It sets up the camera
// view, and updates it based on the data it receives from the Coordinator. Also
// it notifies the coordinator of changes resulting from events on the view.
class CameraViewController {
 public:
  // Gets the combobox selected camera source info.
  using SourceChangeCallback =
      base::RepeatingCallback<void(const VideoSourceInfo& video_source_info)>;

  CameraViewController(MediaView& base_view,
                       SourceChangeCallback callback,
                       bool needs_borders,
                       CameraSelectorComboboxModel& combobox_model);
  CameraViewController(const CameraViewController&) = delete;
  CameraViewController& operator=(const CameraViewController&) = delete;
  ~CameraViewController();

  // `video_source_infos` is  a list of connected devices. When a new device
  // gets connected or a device gets disconnected, this function is called to
  // update the list of devices in the combobox.
  void UpdateVideoSourceInfos(std::vector<VideoSourceInfo> video_source_infos);

  // Returns the immediate parent view of the live camera feed.
  MediaView& GetLiveFeedContainer();

 private:
  friend class CameraViewControllerTest;

  // Called when a combobox selection has been made.
  void OnSourceChanged();

  SourceChangeCallback source_changed_callback_;
  const raw_ref<CameraSelectorComboboxModel> combobox_model_;
  std::unique_ptr<MediaViewControllerBase> base_controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_CAMERA_VIEW_CONTROLLER_H_
