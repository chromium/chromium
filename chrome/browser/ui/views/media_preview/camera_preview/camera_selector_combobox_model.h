// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_CAMERA_SELECTOR_COMBOBOX_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_CAMERA_SELECTOR_COMBOBOX_MODEL_H_

#include <string>
#include <vector>

#include "media/capture/video_capture_types.h"
#include "ui/base/models/combobox_model.h"

namespace media {
struct VideoCaptureDeviceInfo;
}  // namespace media

// A struct to only store relevant info needed about each video source device.
struct VideoSourceInfo {
  explicit VideoSourceInfo(const media::VideoCaptureDeviceInfo& device_info);
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

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_CAMERA_SELECTOR_COMBOBOX_MODEL_H_
