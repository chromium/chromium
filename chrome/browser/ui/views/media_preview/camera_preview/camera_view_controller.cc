// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/camera_preview/camera_view_controller.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "media/capture/video/video_capture_device_info.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_combobox_model.h"

namespace {

std::vector<ui::SimpleComboboxModel::Item> GetComboboxItems(
    const std::vector<media::VideoCaptureDeviceInfo>& video_source_infos) {
  std::vector<ui::SimpleComboboxModel::Item> items;
  items.reserve(video_source_infos.size());
  for (const auto& info : video_source_infos) {
    items.emplace_back(
        /*text=*/base::UTF8ToUTF16(info.descriptor.GetNameAndModel()));
  }
  return items;
}

}  // namespace

CameraViewController::CameraViewController(
    MediaView& base_view,
    bool needs_borders,
    ui::SimpleComboboxModel& combobox_model,
    bool allow_device_selection,
    MediaViewControllerBase::SourceChangeCallback callback,
    media_preview_metrics::Context metrics_context)
    : combobox_model_(combobox_model) {
  const auto& combobox_accessible_name =
      l10n_util::GetStringUTF16(IDS_MEDIA_PREVIEW_CAMERA_ACCESSIBLE_NAME);
  const auto& no_devices_found_combobox_text =
      l10n_util::GetStringUTF16(IDS_MEDIA_PREVIEW_NO_CAMERAS_FOUND_COMBOBOX);
  const auto& no_devices_found_label_text =
      l10n_util::GetStringUTF16(IDS_MEDIA_PREVIEW_NO_CAMERAS_FOUND);

  base_controller_ = std::make_unique<MediaViewControllerBase>(
      base_view, needs_borders, &combobox_model, std::move(callback),
      combobox_accessible_name, no_devices_found_combobox_text,
      no_devices_found_label_text, allow_device_selection, metrics_context);
}

CameraViewController::~CameraViewController() = default;

MediaView& CameraViewController::GetLiveFeedContainer() {
  return base_controller_->GetLiveFeedContainer();
}

void CameraViewController::UpdateVideoSourceInfos(
    const std::vector<media::VideoCaptureDeviceInfo>& video_source_infos) {
  auto video_source_info_count = video_source_infos.size();

  combobox_model_->UpdateItemList(GetComboboxItems(video_source_infos));
  base_controller_->OnDeviceListChanged(video_source_info_count);
}
