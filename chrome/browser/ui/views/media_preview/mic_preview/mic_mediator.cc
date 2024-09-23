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
  devices_observer_.Observe(media_effects::MediaDeviceInfo::GetInstance());
}

MicMediator::~MicMediator() = default;

void MicMediator::GetAudioInputDeviceFormats(
    const std::string& device_id,
    audio::mojom::SystemInfo::GetInputStreamParametersCallback callback) {
  media_effects::MediaDeviceInfo::GetInstance()->GetAudioInputStreamParameters(
      device_id, std::move(callback));
}

void MicMediator::BindAudioStreamFactory(
    mojo::PendingReceiver<media::mojom::AudioStreamFactory>
        audio_stream_factory) {
  content::GetAudioService().BindStreamFactory(std::move(audio_stream_factory));
}

void MicMediator::InitializeDeviceList() {
  // Get current list of audio input devices.
  OnAudioDevicesChanged(
      media_effects::MediaDeviceInfo::GetInstance()->GetAudioDeviceInfos());
}

void MicMediator::OnAudioDevicesChanged(
    const std::optional<std::vector<media::AudioDeviceDescription>>&
        device_infos) {
  if (!device_infos) {
    devices_changed_callback_.Run({});
    return;
  }
  is_device_list_initialized_ = true;
  // Copy into a mutable vector in order to be re-ordered by
  // `PreferenceRankDeviceInfos`.
  auto infos = device_infos.value();
  media_prefs::PreferenceRankAudioDeviceInfos(*prefs_, infos);
  devices_changed_callback_.Run(infos);
}
