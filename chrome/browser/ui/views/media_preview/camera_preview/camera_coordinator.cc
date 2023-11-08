// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/camera_preview/camera_coordinator.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/media_preview/camera_preview/camera_mediator.h"
#include "chrome/browser/ui/views/media_preview/media_view.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/mojom/video_source.mojom.h"

CameraCoordinator::CameraCoordinator(views::View& parent_view,
                                     bool needs_borders)
    : camera_mediator_(
          base::BindRepeating(&CameraCoordinator::OnVideoSourceInfosReceived,
                              base::Unretained(this))) {
  auto* camera_view = parent_view.AddChildView(
      std::make_unique<MediaView>(/*is_subsection=*/false));
  camera_view_tracker_.SetView(camera_view);
  // Safe to use base::Unretained() because `this` owns / outlives
  // `camera_view_tracker_`.
  camera_view_tracker_.SetOnViewIsDeletingCallback(base::BindOnce(
      &CameraCoordinator::ResetViewController, base::Unretained(this)));

  // Safe to use base::Unretained() because `this` owns / outlives
  // `camera_view_controller_`.
  camera_view_controller_.emplace(
      *camera_view,
      base::BindRepeating(&CameraCoordinator::OnVideoSourceChanged,
                          base::Unretained(this)),
      needs_borders, combobox_model_);
}

CameraCoordinator::~CameraCoordinator() = default;

void CameraCoordinator::OnVideoSourceInfosReceived(
    const std::vector<media::VideoCaptureDeviceInfo>& device_infos) {
  if (!camera_view_controller_.has_value()) {
    return;
  }

  std::vector<VideoSourceInfo> relevant_device_infos;
  relevant_device_infos.reserve(device_infos.size());
  for (const auto& device_info : device_infos) {
    relevant_device_infos.emplace_back(
        device_info.descriptor.device_id,
        base::UTF8ToUTF16(device_info.descriptor.GetNameAndModel()),
        device_info.supported_formats);
  }

  camera_view_controller_->UpdateVideoSourceInfos(
      std::move(relevant_device_infos));
}

void CameraCoordinator::OnVideoSourceChanged(
    const VideoSourceInfo& video_source_info) {
  mojo::Remote<video_capture::mojom::VideoSource> video_source;
  camera_mediator_.BindVideoSource(video_source_info.id,
                                   video_source.BindNewPipeAndPassReceiver());
  // TODO(ahmedmoussa): `video_source` is to be passed to
  // VideoStreamCoordiantor. Done in the following CL.
}

void CameraCoordinator::ResetViewController() {
  camera_view_controller_.reset();
}
