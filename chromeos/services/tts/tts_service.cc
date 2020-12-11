// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/tts/tts_service.h"

#include <dlfcn.h>
#include <sys/resource.h>

#include "base/files/file_util.h"
#include "chromeos/services/tts/constants.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_sample_types.h"
#include "services/audio/public/cpp/output_device.h"

namespace chromeos {
namespace tts {

namespace {
constexpr int kDefaultSampleRate = 24000;
constexpr int kDefaultBufferSize = 512;
}  // namespace

TtsService::TtsService(mojo::PendingReceiver<mojom::TtsService> receiver)
    : service_receiver_(this, std::move(receiver)), tts_stream_factory_(this) {
  if (setpriority(PRIO_PROCESS, 0, -10 /* real time audio */) != 0) {
    PLOG(ERROR) << "Unable to request real time priority; performance will be "
                   "impacted.";
  }
}

TtsService::~TtsService() = default;

void TtsService::BindTtsStreamFactory(
    mojo::PendingReceiver<mojom::TtsStreamFactory> receiver,
    mojo::PendingRemote<audio::mojom::StreamFactory> factory) {
  pending_tts_stream_factory_receivers_.push(std::move(receiver));
  ProcessPendingTtsStreamFactories();

  // TODO(accessibility): make it possible to change this dynamically. Also,
  // decouple TtsStreamFactory from AudioStreamFactory above into different
  // calls.
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::CHANNEL_LAYOUT_MONO, kDefaultSampleRate,
                                kDefaultBufferSize);

  output_device_ = std::make_unique<audio::OutputDevice>(
      std::move(factory), params, this, std::string());
}

void TtsService::CreateGoogleTtsStream(CreateGoogleTtsStreamCallback callback) {
  mojo::PendingRemote<mojom::GoogleTtsStream> remote;
  auto receiver = remote.InitWithNewPipeAndPassReceiver();
  google_tts_stream_ =
      std::make_unique<GoogleTtsStream>(this, std::move(receiver));
  std::move(callback).Run(std::move(remote));

  tts_stream_factory_.reset();
  ProcessPendingTtsStreamFactories();
}

void TtsService::CreatePlaybackTtsStream(
    CreatePlaybackTtsStreamCallback callback) {
  mojo::PendingRemote<mojom::PlaybackTtsStream> remote;
  auto receiver = remote.InitWithNewPipeAndPassReceiver();
  playback_tts_stream_ =
      std::make_unique<PlaybackTtsStream>(this, std::move(receiver));
  std::move(callback).Run(std::move(remote), kDefaultSampleRate,
                          kDefaultBufferSize);

  tts_stream_factory_.reset();
  ProcessPendingTtsStreamFactories();
}

void TtsService::Play(
    base::OnceCallback<void(::mojo::PendingReceiver<mojom::TtsEventObserver>)>
        callback) {
  tts_event_observer_.reset();
  auto pending_receiver = tts_event_observer_.BindNewPipeAndPassReceiver();
  std::move(callback).Run(std::move(pending_receiver));

  output_device_->Play();
}

void TtsService::AddAudioBuffer(AudioBuffer buf) {
  base::AutoLock al(state_lock_);
  buffers_.emplace(std::move(buf));
}

void TtsService::AddExplicitTimepoint(int char_index, base::TimeDelta delay) {
  base::AutoLock al(state_lock_);
  timepoints_.push({char_index, delay});
}

void TtsService::Stop() {
  base::AutoLock al(state_lock_);
  StopLocked();
}

void TtsService::SetVolume(float volume) {
  output_device_->SetVolume(volume);
}

void TtsService::Pause() {
  base::AutoLock al(state_lock_);
  StopLocked(false /* clear_buffers */);
}

void TtsService::Resume() {
  output_device_->Play();
}

void TtsService::MaybeExit() {
  if (google_tts_stream_ && !google_tts_stream_->IsBound() &&
      playback_tts_stream_ && !playback_tts_stream_->IsBound()) {
    exit(0);
  }
}

int TtsService::Render(base::TimeDelta delay,
                       base::TimeTicks delay_timestamp,
                       int prior_frames_skipped,
                       media::AudioBus* dest) {
  size_t frames_in_buf = 0;
  int32_t status = -1;
  {
    base::AutoLock al(state_lock_);
    if (buffers_.empty())
      return 0;

    const AudioBuffer& buf = buffers_.front();

    status = buf.status;
    // Done, 0, or error, -1.
    if (status <= 0) {
      if (status == -1)
        tts_event_observer_->OnError();
      else
        tts_event_observer_->OnEnd();

      StopLocked();
      return 0;
    }

    if (buf.is_first_buffer) {
      start_playback_time_ = base::Time::Now();
      tts_event_observer_->OnStart();
    }

    // Implicit timepoint.
    if (buf.char_index != -1)
      tts_event_observer_->OnTimepoint(buf.char_index);

    // Explicit timepoint(s).
    base::TimeDelta start_to_now = base::Time::Now() - start_playback_time_;
    while (!timepoints_.empty() && timepoints_.front().second <= start_to_now) {
      tts_event_observer_->OnTimepoint(timepoints_.front().first);
      timepoints_.pop();
    }

    frames_in_buf = buf.frames.size();
    const float* frames = nullptr;
    if (!buf.frames.empty())
      frames = &buf.frames[0];
    float* channel = dest->channel(0);
    for (size_t i = 0; i < frames_in_buf; i++)
      channel[i] = frames[i];
    buffers_.pop();
  }

  return frames_in_buf;
}

void TtsService::OnRenderError() {}

void TtsService::StopLocked(bool clear_buffers) {
  output_device_->Pause();
  if (clear_buffers) {
    buffers_ = std::queue<AudioBuffer>();
    timepoints_ = std::queue<Timepoint>();
  }
}

void TtsService::ProcessPendingTtsStreamFactories() {
  if (tts_stream_factory_.is_bound() ||
      pending_tts_stream_factory_receivers_.empty())
    return;

  auto factory = std::move(pending_tts_stream_factory_receivers_.front());
  pending_tts_stream_factory_receivers_.pop();
  tts_stream_factory_.Bind(std::move(factory));
}

TtsService::AudioBuffer::AudioBuffer() = default;

TtsService::AudioBuffer::~AudioBuffer() = default;

TtsService::AudioBuffer::AudioBuffer(TtsService::AudioBuffer&& other) {
  frames.swap(other.frames);
  status = other.status;
  char_index = other.char_index;
  is_first_buffer = other.is_first_buffer;
}

}  // namespace tts
}  // namespace chromeos
