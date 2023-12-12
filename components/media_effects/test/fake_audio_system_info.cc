// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_effects/test/fake_audio_system_info.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/system/system_monitor.h"
#include "media/base/audio_parameters.h"

namespace media_effects {

FakeAudioSystemInfo::FakeAudioSystemInfo() = default;

FakeAudioSystemInfo::~FakeAudioSystemInfo() = default;

void FakeAudioSystemInfo::Bind(
    mojo::PendingReceiver<audio::mojom::SystemInfo> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void FakeAudioSystemInfo::AddFakeInputDevice(
    const media::AudioDeviceDescription& descriptor) {
  input_device_descriptions_.emplace(descriptor.unique_id, descriptor);
  NotifyDevicesChanged();
}

void FakeAudioSystemInfo::RemoveFakeInputDevice(const std::string& device_id) {
  input_device_descriptions_.erase(device_id);
  NotifyDevicesChanged();
}

void FakeAudioSystemInfo::GetInputStreamParameters(
    const std::string& device_id,
    GetInputStreamParametersCallback callback) {
  if (input_device_descriptions_.find(device_id) ==
      input_device_descriptions_.end()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  on_get_input_stream_params_callback_.Run(device_id);
  std::move(callback).Run(GetAudioParameters());
}

void FakeAudioSystemInfo::HasInputDevices(HasInputDevicesCallback callback) {
  std::move(callback).Run(input_device_descriptions_.size());
}

void FakeAudioSystemInfo::GetInputDeviceDescriptions(
    GetInputDeviceDescriptionsCallback callback) {
  std::vector<media::AudioDeviceDescription> devices;
  for (const auto& [_, device_info] : input_device_descriptions_) {
    devices.emplace_back(device_info);
  }

  // Simulate the asynchronously behavior of the actual SystemInfo
  // which does a lot of asynchronous and mojo calls.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTaskAndReply(
      FROM_HERE, base::BindOnce(std::move(callback), devices),
      std::exchange(on_replied_with_input_device_descriptions_,
                    base::DoNothing()));
}

void FakeAudioSystemInfo::NotifyDevicesChanged() {
  base::SystemMonitor::Get()->ProcessDevicesChanged(
      base::SystemMonitor::DeviceType::DEVTYPE_AUDIO);
}

// static
media::AudioParameters FakeAudioSystemInfo::GetAudioParameters() {
  // Some arbitrary values, as to guarantee `params`to be valid.
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Mono(),
                                /*sample_rate=*/3300, /*frames_per_buffer=*/40);
  CHECK(params.IsValid());
  return params;
}

}  // namespace media_effects
