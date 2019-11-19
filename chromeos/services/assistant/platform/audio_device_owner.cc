// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/audio_device_owner.h"

#include <algorithm>
#include <utility>

#include "media/audio/audio_device_description.h"
#include "media/base/limits.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace chromeos {
namespace assistant {

namespace {

constexpr int kNumberOfBuffersPerSec = 10;

int32_t GetBytesPerSample(const assistant_client::OutputStreamFormat& format) {
  switch (format.encoding) {
    case assistant_client::OutputStreamEncoding::STREAM_PCM_S16:
      return 2;
    case assistant_client::OutputStreamEncoding::STREAM_PCM_S32:
    case assistant_client::OutputStreamEncoding::STREAM_PCM_F32:
      return 4;
    default:
      break;
  }
  NOTREACHED();
  return 1;
}

int32_t GetBytesPerFrame(const assistant_client::OutputStreamFormat& format) {
  return GetBytesPerSample(format) * format.pcm_num_channels;
}

void FillAudioFifoWithDataOfBufferFormat(
    media::AudioBlockFifo* fifo,
    const std::vector<uint8_t>& data,
    const assistant_client::OutputStreamFormat& output_format,
    int num_bytes) {
  int bytes_per_frame = GetBytesPerFrame(output_format);
  int bytes_per_sample = GetBytesPerSample(output_format);
  int frames = num_bytes / bytes_per_frame;
  fifo->Push(data.data(), frames, bytes_per_sample);
}

int32_t GetBufferSizeInBytesFromBufferFormat(
    const assistant_client::OutputStreamFormat& format) {
  return GetBytesPerFrame(format) * format.pcm_sample_rate /
         kNumberOfBuffersPerSec;
}

media::AudioParameters GetAudioParametersFromBufferFormat(
    const assistant_client::OutputStreamFormat& output_format) {
  DCHECK(output_format.pcm_num_channels <= 2 &&
         output_format.pcm_num_channels > 0);

  return media::AudioParameters(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::GuessChannelLayout(output_format.pcm_num_channels),
      output_format.pcm_sample_rate,
      output_format.pcm_sample_rate / kNumberOfBuffersPerSec);
}

}  // namespace

AudioDeviceOwner::AudioDeviceOwner(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    const std::string& device_id)
    : main_task_runner_(task_runner),
      background_task_runner_(background_task_runner),
      device_id_(device_id) {}

AudioDeviceOwner::~AudioDeviceOwner() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
}

void AudioDeviceOwner::StartOnMainThread(
    assistant_client::AudioOutput::Delegate* delegate,
    mojo::PendingRemote<audio::mojom::StreamFactory> stream_factory,
    const assistant_client::OutputStreamFormat& format) {
  DCHECK(!output_device_);
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());

  delegate_ = delegate;
  format_ = format;
  // TODO(wutao): There is a bug LibAssistant sends wrong format. Do not run
  // in this case.
  if (format_.pcm_num_channels >
      static_cast<int>(media::limits::kMaxChannels)) {
    delegate_->OnEndOfStream();
    return;
  }

  audio_param_ = GetAudioParametersFromBufferFormat(format_);

  // |audio_fifo_| contains 8x the number of frames to render.
  audio_fifo_ = std::make_unique<media::AudioBlockFifo>(
      format.pcm_num_channels, audio_param_.frames_per_buffer(), 8);
  audio_data_.resize(GetBufferSizeInBytesFromBufferFormat(format_));

  {
    base::AutoLock lock(lock_);
    ScheduleFillLocked(base::TimeTicks::Now());
  }

  // |stream_factory| is null in unittest.
  if (stream_factory) {
    // |AudioDeviceOwner| is destroyed on background thread. Thus, it's safe to
    // use base::Unretained.
    background_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&AudioDeviceOwner::StartDeviceOnBackgroundThread,
                       base::Unretained(this), std::move(stream_factory)));
  }
}

void AudioDeviceOwner::StopOnBackgroundThread() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  base::AutoLock lock(lock_);
  output_device_.reset();
  if (delegate_) {
    delegate_->OnStopped();
    delegate_ = nullptr;
  }
}

void AudioDeviceOwner::StartDeviceOnBackgroundThread(
    mojo::PendingRemote<audio::mojom::StreamFactory> stream_factory) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  output_device_ = std::make_unique<audio::OutputDevice>(
      std::move(stream_factory), audio_param_, this, device_id_);
  output_device_->Play();
}

int AudioDeviceOwner::Render(base::TimeDelta delay,
                             base::TimeTicks delay_timestamp,
                             int prior_frames_skipped,
                             media::AudioBus* dest) {
  base::AutoLock lock(lock_);

  if (!is_filling_ && audio_fifo_->GetAvailableFrames() <= 0) {
    if (delegate_)
      delegate_->OnEndOfStream();
    return 0;
  }
  if (audio_fifo_->GetAvailableFrames() <= 0) {
    // Wait for the next round of filling. This should only happen at the
    // very beginning.
    return 0;
  }

  int available_frames = audio_fifo_->GetAvailableFrames();
  if (available_frames < dest->frames()) {
    // In our setting, dest->frames() == frames per block in |audio_fifo_|.
    DCHECK_EQ(audio_fifo_->available_blocks(), 0);

    int frames_to_fill = audio_param_.frames_per_buffer() - available_frames;

    DCHECK_GE(frames_to_fill, 0);

    // Fill up to one block with zero data so that |audio_fifo_| has 1 block
    // to consume. This avoids DCHECK in audio_fifo_->Consume() and also
    // prevents garbage data being copied to |dest| in production.
    audio_fifo_->PushSilence(frames_to_fill);
  }

  audio_fifo_->Consume()->CopyTo(dest);

  ScheduleFillLocked(base::TimeTicks::Now() - delay);
  return dest->frames();
}

void AudioDeviceOwner::OnRenderError() {
  DVLOG(1) << "OnRenderError()";
  base::AutoLock lock(lock_);
  if (delegate_)
    delegate_->OnError(assistant_client::AudioOutput::Error::FATAL_ERROR);
}

void AudioDeviceOwner::SetDelegate(
    assistant_client::AudioOutput::Delegate* delegate) {
  base::AutoLock lock(lock_);
  delegate_ = delegate;
}

void AudioDeviceOwner::ScheduleFillLocked(const base::TimeTicks& time) {
  lock_.AssertAcquired();
  if (is_filling_)
    return;
  is_filling_ = true;
  // FillBuffer will not be called after delegate_->OnEndOfStream, after which
  // AudioDeviceOwner will be destroyed. Thus |this| is valid for capture
  // here.

  if (!delegate_)
    return;

  delegate_->FillBuffer(
      audio_data_.data(),
      std::min(static_cast<int>(audio_data_.size()),
               GetBytesPerFrame(format_) * audio_fifo_->GetUnfilledFrames()),
      time.since_origin().InMicroseconds(),
      [this](int num) { this->BufferFillDone(num); });
}

void AudioDeviceOwner::BufferFillDone(int num_bytes) {
  base::AutoLock lock(lock_);
  is_filling_ = false;
  if (num_bytes == 0)
    return;
  FillAudioFifoWithDataOfBufferFormat(audio_fifo_.get(), audio_data_, format_,
                                      num_bytes);
  if (audio_fifo_->GetUnfilledFrames() > 0)
    ScheduleFillLocked(base::TimeTicks::Now());
}

}  // namespace assistant
}  // namespace chromeos
