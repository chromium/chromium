// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/recording/audio_capturer.h"

#include "services/audio/public/cpp/device_factory.h"

namespace recording {

AudioCapturer::AudioCapturer(
    const std::string& device_id,
    mojo::PendingRemote<media::mojom::AudioStreamFactory> audio_stream_factory,
    const media::AudioParameters& audio_params,
    OnAudioCapturedCallback callback)
    : audio_capturer_(
          audio::CreateInputDevice(std::move(audio_stream_factory),
                                   device_id,
                                   audio::DeadStreamDetection::kEnabled)),
      on_audio_captured_callback_(std::move(callback)) {
  audio_capturer_->Initialize(audio_params, /*callback=*/this);
}

AudioCapturer::~AudioCapturer() = default;

void AudioCapturer::Start() {
  audio_capturer_->Start();
}

void AudioCapturer::Stop() {
  audio_capturer_->Stop();
}

void AudioCapturer::OnCaptureStarted() {}

void AudioCapturer::Capture(const media::AudioBus* audio_source,
                            base::TimeTicks audio_capture_time,
                            double volume,
                            bool key_pressed) {
  // This is called on a worker thread created by the `audio_capturer_` (See
  // `media::AudioDeviceThread`. The given `audio_source` wraps audio data in a
  // shared memory with the audio service. Calling `audio_capturer_->Stop()`
  // will destroy that thread and the shared memory mapping before we get a
  // chance to encode and flush the remaining frames (See
  // `media::AudioInputDevice::Stop()`, and
  // `media::AudioInputDevice::AudioThreadCallback::Process()` for details). It
  // is safer that we own our AudioBuses that are kept alive until encoded and
  // flushed.
  // TODO(b/281868597): Consider using an `AudioBusPool` to avoid doing
  // allocation here on the realtime audio thread.
  auto audio_data =
      media::AudioBus::Create(audio_source->channels(), audio_source->frames());
  audio_source->CopyTo(audio_data.get());

  on_audio_captured_callback_.Run(std::move(audio_data), audio_capture_time);
}

void AudioCapturer::OnCaptureError(media::AudioCapturerSource::ErrorCode code,
                                   const std::string& message) {
  LOG(ERROR) << "AudioCaptureError: code=" << static_cast<uint32_t>(code)
             << ", " << message;
}

void AudioCapturer::OnCaptureMuted(bool is_muted) {}

}  // namespace recording
