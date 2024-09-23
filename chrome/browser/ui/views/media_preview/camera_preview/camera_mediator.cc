// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/camera_preview/camera_mediator.h"
#include "base/functional/bind.h"
#include "chrome/browser/media/prefs/capture_device_ranking.h"
#include "content/public/browser/video_capture_service.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"

CameraMediator::CameraMediator(PrefService& prefs,
                               DevicesChangedCallback devices_changed_callback)
    : prefs_(&prefs),
      devices_changed_callback_(std::move(devices_changed_callback)) {
  devices_observer_.Observe(media_effects::MediaDeviceInfo::GetInstance());

  content::GetVideoCaptureService().ConnectToVideoSourceProvider(
      video_source_provider_.BindNewPipeAndPassReceiver());
  video_source_provider_.reset_on_disconnect();
}

CameraMediator::~CameraMediator() = default;

void CameraMediator::BindVideoSource(
    const std::string& device_id,
    mojo::PendingReceiver<video_capture::mojom::VideoSource> source_receiver) {
  if (video_source_provider_) {
    video_source_provider_->GetVideoSource(device_id,
                                           std::move(source_receiver));
  }
}

void CameraMediator::InitializeDeviceList() {
  // Get current list of video devices.
  OnVideoDevicesChanged(
      media_effects::MediaDeviceInfo::GetInstance()->GetVideoDeviceInfos());
}

void CameraMediator::OnVideoDevicesChanged(
    const std::optional<std::vector<media::VideoCaptureDeviceInfo>>&
        device_infos) {
  if (!device_infos) {
    devices_changed_callback_.Run({});
    return;
  }
  is_device_list_initialized_ = true;
  // Copy into a mutable vector in order to be re-ordered by
  // `PreferenceRankDeviceInfos`.
  auto ranked_device_infos = device_infos.value();
  media_prefs::PreferenceRankVideoDeviceInfos(*prefs_, ranked_device_infos);
  devices_changed_callback_.Run(ranked_device_infos);
}
