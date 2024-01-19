// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/mic_preview/mic_mediator.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/media/prefs/capture_device_ranking.h"
#include "content/public/browser/audio_service.h"

MicMediator::MicMediator(PrefService& prefs,
                         DevicesChangedCallback devices_changed_callback)
    : prefs_(&prefs),
      devices_changed_callback_(std::move(devices_changed_callback)) {
  if (auto* monitor = base::SystemMonitor::Get(); monitor) {
    monitor->AddDevicesChangedObserver(this);
  }

  content::GetAudioService().BindSystemInfo(
      system_info_.BindNewPipeAndPassReceiver());
  system_info_.reset_on_disconnect();
  OnDevicesChanged(base::SystemMonitor::DEVTYPE_AUDIO);
}

void MicMediator::GetAudioInputDeviceFormats(
    const std::string& device_id,
    audio::mojom::SystemInfo::GetInputStreamParametersCallback callback) {
  if (system_info_) {
    system_info_->GetInputStreamParameters(device_id, std::move(callback));
  }
}

void MicMediator::BindAudioStreamFactory(
    mojo::PendingReceiver<media::mojom::AudioStreamFactory>
        audio_stream_factory) {
  content::GetAudioService().BindStreamFactory(std::move(audio_stream_factory));
}

void MicMediator::OnDevicesChanged(
    base::SystemMonitor::DeviceType device_type) {
  if (device_type == base::SystemMonitor::DEVTYPE_AUDIO && system_info_) {
    system_info_->GetInputDeviceDescriptions(base::BindOnce(
        &MicMediator::OnAudioSourceInfosReceived, base::Unretained(this)));
  }
}

void MicMediator::OnAudioSourceInfosReceived(
    std::vector<media::AudioDeviceDescription> device_infos) {
  media_prefs::PreferenceRankAudioDeviceInfos(*prefs_, device_infos);
  devices_changed_callback_.Run(device_infos);
}

MicMediator::~MicMediator() {
  if (auto* monitor = base::SystemMonitor::Get(); monitor) {
    monitor->RemoveDevicesChangedObserver(this);
  }
}
