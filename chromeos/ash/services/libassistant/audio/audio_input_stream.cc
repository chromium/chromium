// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/audio/audio_input_stream.h"

#include "base/notreached.h"
#include "chromeos/ash/services/libassistant/buildflags.h"
#include "chromeos/ash/services/libassistant/public/mojom/audio_input_controller.mojom.h"
#include "chromeos/ash/services/libassistant/public/mojom/platform_delegate.mojom.h"

#if BUILDFLAG(ENABLE_FAKE_ASSISTANT_MICROPHONE)
#include "chromeos/ash/services/libassistant/audio/fake_input_device.h"
#endif  // BUILDFLAG(ENABLE_FAKE_ASSISTANT_MICROPHONE)

namespace ash::libassistant {

namespace {

#if !BUILDFLAG(ENABLE_FAKE_ASSISTANT_MICROPHONE)
audio::DeadStreamDetection ToDeadStreamDetection(bool detect_dead_stream) {
  return detect_dead_stream ? audio::DeadStreamDetection::kEnabled
                            : audio::DeadStreamDetection::kDisabled;
}
#endif  // !BUILDFLAG(ENABLE_FAKE_ASSISTANT_MICROPHONE)

}  // namespace

AudioInputStream::AudioInputStream(
    mojom::PlatformDelegate* delegate,
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

AudioInputStream::~AudioInputStream() {
  Stop();
}

void AudioInputStream::Start() {
  mojo::PendingRemote<media::mojom::AudioStreamFactory> audio_stream_factory;
  delegate_->BindAudioStreamFactory(
      audio_stream_factory.InitWithNewPipeAndPassReceiver());

#if BUILDFLAG(ENABLE_FAKE_ASSISTANT_MICROPHONE)
  source_ = CreateFakeInputDevice();
#else
  source_ =
      audio::CreateInputDevice(std::move(audio_stream_factory), device_id(),
                               ToDeadStreamDetection(detect_dead_stream_));
#endif  // BUILDFLAG(ENABLE_FAKE_ASSISTANT_MICROPHONE)

  source_->Initialize(GetAudioParameters(), capture_callback_);
  source_->Start();
}

void AudioInputStream::Stop() {
  if (source_) {
    source_->Stop();
    source_.reset();
  }
}

media::AudioParameters AudioInputStream::GetAudioParameters() const {
  // AUDIO_PCM_LINEAR and AUDIO_PCM_LOW_LATENCY are the same on CRAS.
  auto result = media::AudioParameters(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Guess(buffer_format_.num_channels),
      buffer_format_.sample_rate,
      buffer_format_.sample_rate / 10 /* buffer size for 100 ms */);

  // Set the HOTWORD mask so CRAS knows the device is used for HOTWORD purpose
  // and is able to conduct the tuning specifically for the scenario. Whether
  // the HOTWORD is conducted by a hotword device or other devices like
  // internal mic will be determined by the device_id passed to CRAS.
  result.set_effects(media::AudioParameters::PlatformEffectsMask::HOTWORD);

  return result;
}

}  // namespace ash::libassistant
