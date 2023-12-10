// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/camera_preview/camera_selector_combobox_model.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "media/capture/video/video_capture_device_info.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model_observer.h"

VideoSourceInfo::VideoSourceInfo(
    const media::VideoCaptureDeviceInfo& device_info)
    : id(device_info.descriptor.device_id),
      name_and_model(
          base::UTF8ToUTF16(device_info.descriptor.GetNameAndModel())),
      supported_formats(device_info.supported_formats) {}

VideoSourceInfo::~VideoSourceInfo() = default;
VideoSourceInfo::VideoSourceInfo(const VideoSourceInfo& other) = default;
VideoSourceInfo::VideoSourceInfo(VideoSourceInfo&& other) = default;

bool VideoSourceInfo::operator==(const VideoSourceInfo& other) const {
  return id == other.id && name_and_model == other.name_and_model &&
         supported_formats == other.supported_formats;
}

CameraSelectorComboboxModel::CameraSelectorComboboxModel() = default;

CameraSelectorComboboxModel::~CameraSelectorComboboxModel() = default;

void CameraSelectorComboboxModel::UpdateDeviceList(
    std::vector<VideoSourceInfo> video_source_infos) {
  video_source_infos_ = std::move(video_source_infos);

  for (auto& observer : observers()) {
    observer.OnComboboxModelChanged(this);
  }
}

size_t CameraSelectorComboboxModel::GetItemCount() const {
  // Item count will always be >= 1. In case of empty `video_source_infos_`, a
  // message is shown to the user to connect a camera.
  return std::max(video_source_infos_.size(), size_t(1));
}

std::u16string CameraSelectorComboboxModel::GetItemAt(size_t index) const {
  if (video_source_infos_.empty()) {
    CHECK_EQ(index, 0U);
    return l10n_util::GetStringUTF16(
        IDS_MEDIA_PREVIEW_NO_CAMERAS_FOUND_COMBOBOX);
  }

  CHECK_LT(index, video_source_infos_.size());
  return video_source_infos_[index].name_and_model;
}

const VideoSourceInfo& CameraSelectorComboboxModel::GetDeviceInfoAt(
    size_t index) const {
  CHECK_LT(index, video_source_infos_.size());
  return video_source_infos_[index];
}
