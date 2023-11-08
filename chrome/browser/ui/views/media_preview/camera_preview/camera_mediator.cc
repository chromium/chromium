// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/camera_preview/camera_mediator.h"
#include "base/functional/bind.h"
#include "content/public/browser/video_capture_service.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"

CameraMediator::CameraMediator(DevicesChangedCallback devices_changed_callback)
    : devices_changed_callback_(std::move(devices_changed_callback)) {
  if (auto* monitor = base::SystemMonitor::Get(); monitor) {
    monitor->AddDevicesChangedObserver(this);
  }

  content::GetVideoCaptureService().ConnectToVideoSourceProvider(
      video_source_provider_.BindNewPipeAndPassReceiver());
  OnDevicesChanged(base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);
}

CameraMediator::~CameraMediator() {
  if (auto* monitor = base::SystemMonitor::Get(); monitor) {
    monitor->RemoveDevicesChangedObserver(this);
  }
}

void CameraMediator::BindVideoSource(
    const std::string& device_id,
    mojo::PendingReceiver<video_capture::mojom::VideoSource> source_receiver) {
  if (video_source_provider_) {
    video_source_provider_->GetVideoSource(device_id,
                                           std::move(source_receiver));
  }
}

void CameraMediator::OnDevicesChanged(
    base::SystemMonitor::DeviceType device_type) {
  if (device_type == base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE &&
      video_source_provider_) {
    video_source_provider_->GetSourceInfos(base::BindOnce(
        &CameraMediator::OnVideoSourceInfosReceived, base::Unretained(this)));
  }
}

void CameraMediator::OnVideoSourceInfosReceived(
    const std::vector<media::VideoCaptureDeviceInfo>& device_infos) {
  devices_changed_callback_.Run(device_infos);
}
