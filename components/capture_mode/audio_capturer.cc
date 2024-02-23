// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/capture_mode/audio_capturer.h"

#include "base/functional/bind.h"
#include "base/sequence_checker.h"
#include "media/base/audio_bus.h"
#include "services/audio/public/cpp/device_factory.h"

namespace capture_mode {

namespace {

constexpr int kPreAllocatedBuses = 5;
constexpr int kMaxBusCapacity = 10;

}  // namespace

AudioCapturer::AudioCapturer(
    std::string_view device_id,
    mojo::PendingRemote<media::mojom::AudioStreamFactory> audio_stream_factory,
    const media::AudioParameters& audio_params,
    OnAudioCapturedCallback callback)
    : audio_capturer_(
          audio::CreateInputDevice(std::move(audio_stream_factory),
                                   std::string(device_id),
                                   audio::DeadStreamDetection::kEnabled)),
      on_audio_captured_callback_(std::move(callback)),
      audio_bus_pool_(audio_params, kPreAllocatedBuses, kMaxBusCapacity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  weak_ptr_this_ = weak_ptr_factory_.GetWeakPtr();
  audio_capturer_->Initialize(audio_params, /*callback=*/this);
}

AudioCapturer::~AudioCapturer() = default;

void AudioCapturer::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  audio_capturer_->Start();
}

void AudioCapturer::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  audio_capturer_->Stop();
}

void AudioCapturer::OnCaptureStarted() {}

void AudioCapturer::Capture(const media::AudioBus* audio_source,
                            base::TimeTicks audio_capture_time,
                            const media::AudioGlitchInfo& glitch_info,
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
  // We need to avoid allocating new audio buses here, since this is running on
  // the realtime audio thread, hence we use the audio bus pool.
  auto backing_audio_bus = audio_bus_pool_.GetAudioBus();
  DCHECK_EQ(backing_audio_bus->frames(), audio_source->frames());
  DCHECK_EQ(backing_audio_bus->channels(), audio_source->channels());
  audio_source->CopyTo(backing_audio_bus.get());

  // However, the audio bus pool requires that we return back the audio bus when
  // we're done with it, so it can be reused. Therefore we will create another
  // audio bus that acts as a wrapper around the `backing_audio_bus` we got from
  // the pool. The `wrapping_audio_bus` will be the one we provide to the client
  // via `on_audio_captured_callback_`. Once the client is done with it, and it
  // goes out of scope, the `backing_audio_bus` will be returned back to the
  // pool in `OnAudioBusDone()`.
  auto wrapping_audio_bus =
      media::AudioBus::CreateWrapper(backing_audio_bus->channels());
  wrapping_audio_bus->set_frames(backing_audio_bus->frames());
  for (int channel = 0; channel < backing_audio_bus->channels(); ++channel) {
    wrapping_audio_bus->SetChannelData(channel,
                                       backing_audio_bus->channel(channel));
  }
  // Keep `backing_audio_bus` alive as long as `wrapping_audio_bus` by
  // transferring its ownership to the `wrapping_audio_bus`'s deleter callback.
  wrapping_audio_bus->SetWrappedDataDeleter(
      base::BindOnce(&AudioCapturer::OnAudioBusDone, weak_ptr_this_,
                     std::move(backing_audio_bus)));

  on_audio_captured_callback_.Run(std::move(wrapping_audio_bus),
                                  audio_capture_time);
}

void AudioCapturer::OnCaptureError(media::AudioCapturerSource::ErrorCode code,
                                   const std::string& message) {
  LOG(ERROR) << "AudioCaptureError: code=" << static_cast<uint32_t>(code)
             << ", " << message;
}

void AudioCapturer::OnCaptureMuted(bool is_muted) {}

void AudioCapturer::OnAudioBusDone(
    std::unique_ptr<media::AudioBus> backing_audio_bus) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  audio_bus_pool_.InsertAudioBus(std::move(backing_audio_bus));
}

}  // namespace capture_mode
