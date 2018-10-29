// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/audio_input_provider_impl.h"

#include <utility>

#include "base/logging.h"
#include "base/stl_util.h"
#include "libassistant/shared/public/platform_audio_buffer.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_sample_types.h"
#include "media/base/channel_layout.h"
#include "services/audio/public/cpp/device_factory.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromeos {
namespace assistant {

namespace {

// This format should match //c/b/c/assistant/platform_audio_input_host.cc.
constexpr assistant_client::BufferFormat kFormat{
    16000 /* sample_rate */, assistant_client::INTERLEAVED_S32, 1 /* channels */
};

}  // namespace

AudioInputBufferImpl::AudioInputBufferImpl(const void* data,
                                           uint32_t frame_count)
    : data_(data), frame_count_(frame_count) {}

AudioInputBufferImpl::~AudioInputBufferImpl() = default;

assistant_client::BufferFormat AudioInputBufferImpl::GetFormat() const {
  return kFormat;
}

const void* AudioInputBufferImpl::GetData() const {
  return data_;
}

void* AudioInputBufferImpl::GetWritableData() {
  NOTREACHED();
  return nullptr;
}

int AudioInputBufferImpl::GetFrameCount() const {
  return frame_count_;
}

AudioInputImpl::AudioInputImpl(
    std::unique_ptr<service_manager::Connector> connector,
    bool default_on)
    : source_(audio::CreateInputDevice(
          std::move(connector),
          media::AudioDeviceDescription::kDefaultDeviceId)),
      default_on_(default_on),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_factory_(this) {
  DETACH_FROM_SEQUENCE(observer_sequence_checker_);
  // AUDIO_PCM_LINEAR and AUDIO_PCM_LOW_LATENCY are the same on CRAS.
  source_->Initialize(
      media::AudioParameters(
          media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
          media::CHANNEL_LAYOUT_MONO, kFormat.sample_rate,
          kFormat.sample_rate / 10 /* buffer size for 100 ms */),
      this);
}

AudioInputImpl::~AudioInputImpl() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  source_->Stop();
  VLOG(1) << "Ending captured frames: " << captured_frames_count_;
}

void AudioInputImpl::Capture(const media::AudioBus* audio_source,
                             int audio_delay_milliseconds,
                             double volume,
                             bool key_pressed) {
  DCHECK_EQ(kFormat.num_channels, audio_source->channels());
  std::vector<int32_t> buffer(kFormat.num_channels * audio_source->frames());
  audio_source->ToInterleaved<media::SignedInt32SampleTypeTraits>(
      audio_source->frames(), buffer.data());
  int64_t time = base::TimeTicks::Now().since_origin().InMilliseconds() -
                 audio_delay_milliseconds;
  AudioInputBufferImpl input_buffer(buffer.data(), audio_source->frames());
  {
    base::AutoLock lock(lock_);
    for (auto* observer : observers_)
      observer->OnBufferAvailable(input_buffer, time);
  }

  captured_frames_count_ += audio_source->frames();
  if (VLOG_IS_ON(1)) {
    auto now = base::TimeTicks::Now();
    if ((now - last_frame_count_report_time_) >
        base::TimeDelta::FromMinutes(2)) {
      VLOG(1) << "Captured frames: " << captured_frames_count_;
      last_frame_count_report_time_ = now;
    }
  }
}

void AudioInputImpl::OnCaptureError(const std::string& message) {
  LOG(ERROR) << "Capture error " << message;
  base::AutoLock lock(lock_);
  for (auto* observer : observers_)
    observer->OnError(AudioInput::Error::FATAL_ERROR);
}

void AudioInputImpl::OnCaptureMuted(bool is_muted) {}

assistant_client::BufferFormat AudioInputImpl::GetFormat() const {
  return kFormat;
}

void AudioInputImpl::AddObserver(
    assistant_client::AudioInput::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(observer_sequence_checker_);
  VLOG(1) << "Add observer";
  bool should_start = false;
  {
    base::AutoLock lock(lock_);
    observers_.push_back(observer);
    should_start = observers_.size() == 1;
  }

  if (default_on_ && should_start) {
    // Post to main thread runner to start audio recording. Assistant thread
    // does not have thread context defined in //base and will fail sequence
    // check in AudioCapturerSource::Start().
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&AudioInputImpl::StartRecording,
                                          weak_factory_.GetWeakPtr()));
  }
}

void AudioInputImpl::RemoveObserver(
    assistant_client::AudioInput::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(observer_sequence_checker_);
  VLOG(1) << "Remove observer";
  bool should_stop = false;
  {
    base::AutoLock lock(lock_);
    base::Erase(observers_, observer);
    should_stop = observers_.empty();
  }
  if (should_stop) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&AudioInputImpl::StopRecording,
                                          weak_factory_.GetWeakPtr()));

    // Reset the sequence checker since assistant may call from different thread
    // after restart.
    DETACH_FROM_SEQUENCE(observer_sequence_checker_);
  }
}

void AudioInputImpl::SetMicState(bool mic_open) {
  if (!default_on_) {
    if (mic_open) {
      task_runner_->PostTask(FROM_HERE,
                             base::BindOnce(&AudioInputImpl::StartRecording,
                                            weak_factory_.GetWeakPtr()));
    } else {
      task_runner_->PostTask(FROM_HERE,
                             base::BindOnce(&AudioInputImpl::StopRecording,
                                            weak_factory_.GetWeakPtr()));
    }
  }
}

void AudioInputImpl::OnHotwordEnabled(bool enable) {
  default_on_ = enable;
  if (default_on_) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&AudioInputImpl::StartRecording,
                                          weak_factory_.GetWeakPtr()));
  } else {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&AudioInputImpl::StopRecording,
                                          weak_factory_.GetWeakPtr()));
  }
}

void AudioInputImpl::StartRecording() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  VLOG(1) << "Start recording";
  source_->Start();
}

void AudioInputImpl::StopRecording() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  VLOG(1) << "Stop recording";
  source_->Stop();
}

AudioInputProviderImpl::AudioInputProviderImpl(
    service_manager::Connector* connector,
    bool default_on)
    : audio_input_(connector->Clone(), default_on) {}

AudioInputProviderImpl::~AudioInputProviderImpl() = default;

assistant_client::AudioInput& AudioInputProviderImpl::GetAudioInput() {
  return audio_input_;
}

int64_t AudioInputProviderImpl::GetCurrentAudioTime() {
  // TODO(xiaohuic): see if we can support real timestamp.
  return 0;
}

void AudioInputProviderImpl::SetMicState(bool mic_open) {
  audio_input_.SetMicState(mic_open);
}

void AudioInputProviderImpl::OnHotwordEnabled(bool enable) {
  audio_input_.OnHotwordEnabled(enable);
}

}  // namespace assistant
}  // namespace chromeos
