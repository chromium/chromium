// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/tts/tts_player.h"

namespace chromeos {
namespace tts {

TtsPlayer::TtsPlayer(
    mojo::PendingRemote<media::mojom::AudioStreamFactory> factory,
    const media::AudioParameters& params)
    : output_device_(std::move(factory), params, this, std::string()),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {}

TtsPlayer::~TtsPlayer() = default;

void TtsPlayer::Play(
    base::OnceCallback<void(::mojo::PendingReceiver<mojom::TtsEventObserver>)>
        callback) {
  tts_event_observer_.reset();
  auto pending_receiver = tts_event_observer_.BindNewPipeAndPassReceiver();
  std::move(callback).Run(std::move(pending_receiver));

  output_device_.Play();
}

void TtsPlayer::AddAudioBuffer(AudioBuffer buf) {
  base::AutoLock al(state_lock_);
  buffers_.emplace(std::move(buf));
}

void TtsPlayer::AddExplicitTimepoint(int char_index, base::TimeDelta delay) {
  base::AutoLock al(state_lock_);
  timepoints_.push({char_index, delay});
}

void TtsPlayer::Stop() {
  base::AutoLock al(state_lock_);
  StopLocked();
}

void TtsPlayer::SetVolume(float volume) {
  output_device_.SetVolume(volume);
}

void TtsPlayer::Pause() {
  base::AutoLock al(state_lock_);
  StopLocked(false /* clear_buffers */);
}

void TtsPlayer::Resume() {
  output_device_.Play();
}

int TtsPlayer::Render(base::TimeDelta delay,
                      base::TimeTicks delay_timestamp,
                      const media::AudioGlitchInfo& glitch_info,
                      media::AudioBus* dest) {
  size_t frames_in_buf = 0;
  {
    base::AutoLock al(state_lock_);
    if (buffers_.empty())
      return 0;

    const AudioBuffer& buf = buffers_.front();

    frames_in_buf = buf.frames.size();
    const float* frames = nullptr;
    if (!buf.frames.empty())
      frames = &buf.frames[0];
    float* channel = dest->channel(0);
    for (size_t i = 0; i < frames_in_buf; i++)
      channel[i] = frames[i];

    rendered_buffers_.push(std::move(buffers_.front()));
    buffers_.pop();

    if (!process_rendered_buffers_posted_) {
      process_rendered_buffers_posted_ = true;
      task_runner_->PostTask(FROM_HERE,
                             base::BindOnce(&TtsPlayer::ProcessRenderedBuffers,
                                            weak_factory_.GetWeakPtr()));
    }
  }

  return frames_in_buf;
}

void TtsPlayer::OnRenderError() {}

void TtsPlayer::StopLocked(bool clear_buffers) {
  output_device_.Pause();
  rendered_buffers_ = std::queue<AudioBuffer>();
  if (clear_buffers) {
    buffers_ = std::queue<AudioBuffer>();
    timepoints_ = std::queue<Timepoint>();
  }
}

void TtsPlayer::ProcessRenderedBuffers() {
  base::AutoLock al(state_lock_);
  process_rendered_buffers_posted_ = false;
  for (; !rendered_buffers_.empty(); rendered_buffers_.pop()) {
    const auto& buf = rendered_buffers_.front();
    int status = buf.status;
    // Done, 0, or error, -1.
    if (status <= 0) {
      if (status == -1)
        tts_event_observer_->OnError();
      else
        tts_event_observer_->OnEnd();

      StopLocked();
      return;
    }

    if (buf.is_first_buffer) {
      start_playback_time_ = base::Time::Now();
      tts_event_observer_->OnStart();
    }

    // Implicit timepoint.
    if (buf.char_index != -1)
      tts_event_observer_->OnTimepoint(buf.char_index);
  }

  // Explicit timepoint(s).
  base::TimeDelta start_to_now = base::Time::Now() - start_playback_time_;
  while (!timepoints_.empty() && timepoints_.front().second <= start_to_now) {
    tts_event_observer_->OnTimepoint(timepoints_.front().first);
    timepoints_.pop();
  }
}

TtsPlayer::AudioBuffer::AudioBuffer() = default;

TtsPlayer::AudioBuffer::~AudioBuffer() = default;

TtsPlayer::AudioBuffer::AudioBuffer(TtsPlayer::AudioBuffer&& other) {
  frames.swap(other.frames);
  status = other.status;
  char_index = other.char_index;
  is_first_buffer = other.is_first_buffer;
}

}  // namespace tts
}  // namespace chromeos
