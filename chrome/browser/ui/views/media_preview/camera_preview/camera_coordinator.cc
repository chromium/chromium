// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/camera_preview/camera_coordinator.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/ui/views/media_preview/camera_preview/camera_mediator.h"
#include "chrome/browser/ui/views/media_preview/media_view.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/mojom/video_source.mojom.h"

CameraCoordinator::CameraCoordinator(views::View& parent_view,
                                     bool needs_borders)
    : camera_mediator_(
          base::BindRepeating(&CameraCoordinator::OnVideoSourceInfosReceived,
                              base::Unretained(this))) {
  auto* camera_view = parent_view.AddChildView(std::make_unique<MediaView>());
  camera_view_tracker_.SetView(camera_view);
  // Safe to use base::Unretained() because `this` owns / outlives
  // `camera_view_tracker_`.
  camera_view_tracker_.SetOnViewIsDeletingCallback(base::BindOnce(
      &CameraCoordinator::ResetViewController, base::Unretained(this)));

  // Safe to use base::Unretained() because `this` owns / outlives
  // `camera_view_controller_`.
  camera_view_controller_.emplace(
      *camera_view, needs_borders, combobox_model_,
      base::BindRepeating(&CameraCoordinator::OnVideoSourceChanged,
                          base::Unretained(this)));

  video_stream_coordinator_.emplace(
      camera_view_controller_->GetLiveFeedContainer());
}

CameraCoordinator::~CameraCoordinator() {
  // As to guarantee that VideoSourceProvider outlive its VideoSource
  // connection, it is passed in here to protect from destruction.
  video_stream_coordinator_->StopAndCleanup(
      camera_mediator_.TakeVideoSourceProvider());
}

void CameraCoordinator::OnVideoSourceInfosReceived(
    const std::vector<media::VideoCaptureDeviceInfo>& device_infos) {
  if (!camera_view_controller_.has_value()) {
    return;
  }

  std::vector<VideoSourceInfo> relevant_device_infos;
  relevant_device_infos.reserve(device_infos.size());
  for (const auto& device_info : device_infos) {
    relevant_device_infos.emplace_back(device_info);
  }

  if (relevant_device_infos.empty()) {
    active_device_id_.clear();
    video_stream_coordinator_->Stop();
  }
  camera_view_controller_->UpdateVideoSourceInfos(
      std::move(relevant_device_infos));
}

void CameraCoordinator::OnVideoSourceChanged(
    std::optional<size_t> selected_index) {
  if (!selected_index.has_value()) {
    return;
  }

  const auto& device_info =
      combobox_model_.GetDeviceInfoAt(selected_index.value());
  if (active_device_id_ == device_info.id) {
    return;
  }

  active_device_id_ = device_info.id;
  mojo::Remote<video_capture::mojom::VideoSource> video_source;
  camera_mediator_.BindVideoSource(device_info.id,
                                   video_source.BindNewPipeAndPassReceiver());
  video_stream_coordinator_->ConnectToDevice(std::move(video_source),
                                             device_info.supported_formats);
}

void CameraCoordinator::ResetViewController() {
  camera_view_controller_.reset();
}
