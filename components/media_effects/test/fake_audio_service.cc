// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_effects/test/fake_audio_service.h"

#include <utility>

namespace media_effects {

FakeAudioService::FakeAudioService() = default;

FakeAudioService::~FakeAudioService() = default;

void FakeAudioService::AddFakeInputDevice(
    const media::AudioDeviceDescription& descriptor) {
  fake_system_info_.AddFakeInputDevice(descriptor);
}

void FakeAudioService::RemoveFakeInputDevice(const std::string& device_id) {
  fake_system_info_.RemoveFakeInputDevice(device_id);
}

void FakeAudioService::SetOnRepliedWithInputDeviceDescriptionsCallback(
    base::OnceClosure callback) {
  fake_system_info_.SetOnRepliedWithInputDeviceDescriptionsCallback(
      std::move(callback));
}

void FakeAudioService::SetOnGetInputStreamParametersCallback(
    base::RepeatingCallback<void(const std::string&)> callback) {
  fake_system_info_.SetOnGetInputStreamParametersCallback(std::move(callback));
}

void FakeAudioService::BindSystemInfo(
    mojo::PendingReceiver<audio::mojom::SystemInfo> receiver) {
  fake_system_info_.Bind(std::move(receiver));
}

void FakeAudioService::BindStreamFactory(
    mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver) {
  on_bind_stream_factory_callback_.Run();
}

}  // namespace media_effects
