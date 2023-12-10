// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/camera_preview/camera_view_controller.h"

#include <memory>

#include "chrome/browser/ui/views/media_preview/camera_preview/camera_selector_combobox_model.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

CameraViewController::CameraViewController(
    MediaView& base_view,
    bool needs_borders,
    CameraSelectorComboboxModel& combobox_model,
    MediaViewControllerBase::SourceChangeCallback callback)
    : combobox_model_(combobox_model) {
  const auto& combobox_accessible_name =
      l10n_util::GetStringUTF16(IDS_MEDIA_PREVIEW_CAMERA_ACCESSIBLE_NAME);
  const auto& no_device_connected_label_text =
      l10n_util::GetStringUTF16(IDS_MEDIA_PREVIEW_NO_CAMERAS_FOUND);

  base_controller_ = std::make_unique<MediaViewControllerBase>(
      base_view, needs_borders, &combobox_model, std::move(callback),
      combobox_accessible_name, no_device_connected_label_text);
}

CameraViewController::~CameraViewController() = default;

MediaView& CameraViewController::GetLiveFeedContainer() {
  return base_controller_->GetLiveFeedContainer();
}

void CameraViewController::UpdateVideoSourceInfos(
    std::vector<VideoSourceInfo> video_source_infos) {
  bool has_devices = !video_source_infos.empty();
  combobox_model_->UpdateDeviceList(std::move(video_source_infos));
  base_controller_->AdjustComboboxEnabledState(has_devices);
}
