// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/audio_stream.h"
#include "base/notreached.h"
#include "chromeos/services/assistant/buildflags.h"
#include "chromeos/services/assistant/platform/audio_stream_factory_delegate.h"

#if BUILDFLAG(ENABLE_FAKE_ASSISTANT_MICROPHONE)
#include "chromeos/services/assistant/platform/fake_input_device.h"
#endif  // BUILDFLAG(ENABLE_FAKE_ASSISTANT_MICROPHONE)

namespace chromeos {
namespace assistant {

namespace {

media::ChannelLayout GetChannelLayout(
    const assistant_client::BufferFormat& format) {
  switch (format.num_channels) {
    case 1:
      return media::ChannelLayout::CHANNEL_LAYOUT_MONO;
    case 2:
      return media::ChannelLayout::CHANNEL_LAYOUT_STEREO;
    default:
      NOTREACHED();
      return media::ChannelLayout::CHANNEL_LAYOUT_UNSUPPORTED;
  }
}

}  // namespace

AudioStream::AudioStream(
    AudioStreamFactoryDelegate* delegate,
    const std::string& device_id,
    bool detect_dead_stream,
    assistant_client::BufferFormat buffer_format,
    media::AudioCapturerSource::CaptureCallback* capture_callback)
    : device_id_(device_id),
      detect_dead_stream_(detect_dead_stream),
      buffer_format_(buffer_format),
      delegate_(delegate),
      capture_callback_(capture_callback) {
  Start();
}

AudioStream::~AudioStream() {
  Stop();
}

const std::string& AudioStream::device_id() const {
  return device_id_;
}

bool AudioStream::has_dead_stream_detection() const {
  return detect_dead_stream_;
}

void AudioStream::Start() {
  delegate_->RequestAudioStreamFactory(base::BindOnce(
      &AudioStream::OnAudioSteamFactoryReady, weak_ptr_factory_.GetWeakPtr()));
}

void AudioStream::OnAudioSteamFactoryReady(
    mojo::PendingRemote<audio::mojom::StreamFactory> audio_stream_factory) {
#if BUILDFLAG(ENABLE_FAKE_ASSISTANT_MICROPHONE)
  source_ = CreateFakeInputDevice();
#else
  source_ = audio::CreateInputDevice(std::move(audio_stream_factory),
                                     device_id(), DeadStreamDetection());
#endif  // BUILDFLAG(ENABLE_FAKE_ASSISTANT_MICROPHONE)

  source_->Initialize(GetAudioParameters(), capture_callback_);
  source_->Start();
}

void AudioStream::Stop() {
  if (source_) {
    source_->Stop();
    source_.reset();
  }
}

audio::DeadStreamDetection AudioStream::DeadStreamDetection() const {
  return detect_dead_stream_ ? audio::DeadStreamDetection::kEnabled
                             : audio::DeadStreamDetection::kDisabled;
}

media::AudioParameters AudioStream::GetAudioParameters() const {
  // Provide buffer size for 100 ms
  int frames_per_buffer = buffer_format_.sample_rate / 10;

  // AUDIO_PCM_LINEAR and AUDIO_PCM_LOW_LATENCY are the same on CRAS.
  auto result =
      media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                             GetChannelLayout(buffer_format_),
                             buffer_format_.sample_rate, frames_per_buffer);

  // Set the HOTWORD mask so CRAS knows the device is used for HOTWORD purpose
  // and is able to conduct the tuning specifically for the scenario. Whether
  // the HOTWORD is conducted by a hotword device or other devices like
  // internal mic will be determined by the device_id passed to CRAS.
  result.set_effects(media::AudioParameters::PlatformEffectsMask::HOTWORD);

  return result;
}

}  // namespace assistant
}  // namespace chromeos
