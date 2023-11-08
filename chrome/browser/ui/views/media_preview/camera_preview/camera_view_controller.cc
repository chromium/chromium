// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/camera_preview/camera_view_controller.h"

#include <algorithm>
#include <memory>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/combobox_model_observer.h"

VideoSourceInfo::VideoSourceInfo(
    const std::string& id,
    const std::u16string& name_and_model,
    const media::VideoCaptureFormats& supported_formats)
    : id(id),
      name_and_model(name_and_model),
      supported_formats(supported_formats) {}

VideoSourceInfo::~VideoSourceInfo() = default;
VideoSourceInfo::VideoSourceInfo(const VideoSourceInfo& other) = default;
VideoSourceInfo::VideoSourceInfo(VideoSourceInfo&& other) = default;

bool VideoSourceInfo::operator==(const VideoSourceInfo& other) const {
  return id == other.id && name_and_model == other.name_and_model &&
         supported_formats == other.supported_formats;
}

CameraViewController::CameraViewController(
    MediaView& base_view,
    SourceChangeCallback callback,
    bool needs_borders,
    CameraSelectorComboboxModel& combobox_model)
    : source_changed_callback_(std::move(callback)),
      combobox_model_(combobox_model) {
  CHECK(source_changed_callback_);
  const auto& combobox_accessible_name =
      l10n_util::GetStringUTF16(IDS_MEDIA_PREVIEW_CAMERA_ACCESSIBLE_NAME);
  const auto& no_device_connected_label_text =
      l10n_util::GetStringUTF16(IDS_MEDIA_PREVIEW_NO_CAMERAS_FOUND);

  // Unretained is safe, because `base_controller_` is owned by `this`, and
  // the combobox's callback is cleared in ~BaseMediaViewController.
  base_controller_ = std::make_unique<MediaViewControllerBase>(
      base_view, needs_borders, &combobox_model_.get(),
      base::BindRepeating(&CameraViewController::OnSourceChanged,
                          base::Unretained(this)),
      combobox_accessible_name, no_device_connected_label_text);
}

CameraViewController::~CameraViewController() = default;

void CameraViewController::UpdateVideoSourceInfos(
    std::vector<VideoSourceInfo> video_source_infos) {
  bool has_devices = !video_source_infos.empty();
  combobox_model_->UpdateDeviceList(std::move(video_source_infos));
  base_controller_->AdjustComboboxEnabledState(has_devices);
}

MediaView& CameraViewController::GetLiveFeedContainer() {
  return base_controller_->GetLiveFeedContainer();
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

void CameraViewController::OnSourceChanged() {
  const auto& camera_index = base_controller_->GetComboboxSelectedIndex();
  if (!camera_index.has_value()) {
    return;
  }

  const auto& device_info =
      combobox_model_->GetDeviceInfoAt(camera_index.value());
  if (base_controller_->UpdateActiveDeviceId(device_info.id)) {
    source_changed_callback_.Run(device_info);
  }
}
