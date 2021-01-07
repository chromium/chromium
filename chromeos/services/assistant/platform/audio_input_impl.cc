// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/audio_input_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/timer/timer.h"
#include "chromeos/services/assistant/platform/audio_stream.h"
#include "chromeos/services/assistant/platform/audio_stream_factory_delegate.h"
#include "chromeos/services/assistant/public/cpp/assistant_client.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "chromeos/services/assistant/utils.h"
#include "libassistant/shared/public/platform_audio_buffer.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_sample_types.h"
#include "media/base/channel_layout.h"
#include "services/audio/public/cpp/device_factory.h"
#include "services/audio/public/mojom/stream_factory.mojom.h"

namespace chromeos {
namespace assistant {

namespace {

constexpr assistant_client::BufferFormat kFormatMono{
    16000 /* sample_rate */, assistant_client::INTERLEAVED_S16, 1 /* channels */
};

constexpr assistant_client::BufferFormat kFormatStereo{
    44100 /* sample_rate */, assistant_client::INTERLEAVED_S16, 2 /* channels */
};

assistant_client::BufferFormat g_current_format = kFormatMono;

class DspHotwordStateManager : public AudioInputImpl::HotwordStateManager {
 public:
  DspHotwordStateManager(AudioInputImpl* input,
                         scoped_refptr<base::SequencedTaskRunner> task_runner)
      : AudioInputImpl::HotwordStateManager(input), task_runner_(task_runner) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
  }

  // HotwordStateManager overrides:
  // Runs on main thread.
  void OnConversationTurnStarted() override {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    if (second_phase_timer_.IsRunning()) {
      DCHECK(stream_state_ == StreamState::HOTWORD);
      second_phase_timer_.Stop();
    } else {
      // Handles user click on mic button.
      input_->RecreateAudioInputStream(false /* use_dsp */);
    }
    stream_state_ = StreamState::NORMAL;
  }

  // Runs on main thread.
  void OnConversationTurnFinished() override {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    input_->RecreateAudioInputStream(true /* use_dsp */);
    if (stream_state_ == StreamState::HOTWORD) {
      // If |stream_state_| remains unchanged, that indicates the first stage
      // DSP hotword detection was rejected by Libassistant.
      RecordDspHotwordDetection(DspHotwordDetectionStatus::SOFTWARE_REJECTED);
    }
    stream_state_ = StreamState::HOTWORD;
  }

  // Runs on audio service thread
  void OnCaptureDataArrived() override {
    // Posting to main thread to avoid timer's sequence check error.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&DspHotwordStateManager::OnCaptureDataArrivedMainThread,
                       weak_factory_.GetWeakPtr()));
  }

  void RecreateAudioInputStream() override {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    input_->RecreateAudioInputStream(stream_state_ == StreamState::HOTWORD);
  }

  // Runs on main thread.
  void OnCaptureDataArrivedMainThread() {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    if (stream_state_ == StreamState::HOTWORD &&
        !second_phase_timer_.IsRunning()) {
      RecordDspHotwordDetection(DspHotwordDetectionStatus::HARDWARE_ACCEPTED);
      // 1s from now, if OnConversationTurnStarted is not called, we assume that
      // libassistant has rejected the hotword supplied by DSP. Thus, we reset
      // and reopen the device on hotword state.
      second_phase_timer_.Start(
          FROM_HERE, base::TimeDelta::FromSeconds(1),
          base::BindRepeating(
              &DspHotwordStateManager::OnConversationTurnFinished,
              base::Unretained(this)));
    }
  }

 private:
  enum class StreamState {
    HOTWORD,
    NORMAL,
  };

  // Defines possible detection states of Dsp hotword. These values are
  // persisted to logs. Entries should not be renumbered and numeric values
  // should never be reused. Only append to this enum is allowed if the possible
  // source grows.
  enum class DspHotwordDetectionStatus {
    HARDWARE_ACCEPTED = 0,
    SOFTWARE_REJECTED = 1,
    kMaxValue = SOFTWARE_REJECTED
  };

  // Helper function to record UMA metrics for Dsp hotword detection.
  void RecordDspHotwordDetection(DspHotwordDetectionStatus status) {
    base::UmaHistogramEnumeration("Assistant.DspHotwordDetection", status);
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  StreamState stream_state_ = StreamState::HOTWORD;
  base::OneShotTimer second_phase_timer_;
  base::WeakPtrFactory<DspHotwordStateManager> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DspHotwordStateManager);
};

class AudioInputBufferImpl : public assistant_client::AudioBuffer {
 public:
  AudioInputBufferImpl(const void* data, uint32_t frame_count)
      : data_(data), frame_count_(frame_count) {}
  ~AudioInputBufferImpl() override = default;

  // assistant_client::AudioBuffer overrides:
  assistant_client::BufferFormat GetFormat() const override {
    return g_current_format;
  }
  const void* GetData() const override { return data_; }
  void* GetWritableData() override {
    NOTREACHED();
    return nullptr;
  }
  int GetFrameCount() const override { return frame_count_; }

 private:
  const void* data_;
  int frame_count_;
  DISALLOW_COPY_AND_ASSIGN(AudioInputBufferImpl);
};

}  // namespace

AudioInputImpl::HotwordStateManager::HotwordStateManager(
    AudioInputImpl* audio_input)
    : input_(audio_input) {}

void AudioInputImpl::HotwordStateManager::RecreateAudioInputStream() {
  input_->RecreateAudioInputStream(/*use_dsp=*/false);
}

AudioInputImpl::AudioInputImpl(
    AudioStreamFactoryDelegate* audio_stream_factory_delegate,
    const std::string& device_id)
    : task_runner_(base::SequencedTaskRunnerHandle::Get()),
      audio_stream_factory_delegate_(audio_stream_factory_delegate),
      preferred_device_id_(device_id),
      weak_factory_(this) {
  DETACH_FROM_SEQUENCE(observer_sequence_checker_);

  DCHECK(audio_stream_factory_delegate_);

  RecreateStateManager();
  if (features::IsStereoAudioInputEnabled())
    g_current_format = kFormatStereo;
  else
    g_current_format = kFormatMono;
}

AudioInputImpl::~AudioInputImpl() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  StopRecording();
}

void AudioInputImpl::RecreateStateManager() {
  if (IsHotwordAvailable()) {
    state_manager_ =
        std::make_unique<DspHotwordStateManager>(this, task_runner_);
  } else {
    state_manager_ = std::make_unique<HotwordStateManager>(this);
  }
}

// Runs on audio service thread.
void AudioInputImpl::Capture(const media::AudioBus* audio_source,
                             base::TimeTicks audio_capture_time,
                             double volume,
                             bool key_pressed) {
  DCHECK_EQ(g_current_format.num_channels, audio_source->channels());

  state_manager_->OnCaptureDataArrived();

  std::vector<int16_t> buffer(audio_source->channels() *
                              audio_source->frames());
  audio_source->ToInterleaved<media::SignedInt16SampleTypeTraits>(
      audio_source->frames(), buffer.data());
  int64_t time = 0;
  // Only provide accurate timestamp when eraser is enabled, otherwise it seems
  // break normal libassistant voice recognition.
  if (features::IsAudioEraserEnabled())
    time = audio_capture_time.since_origin().InMicroseconds();
  AudioInputBufferImpl input_buffer(buffer.data(), audio_source->frames());
  {
    base::AutoLock lock(lock_);
    for (auto* observer : observers_)
      observer->OnAudioBufferAvailable(input_buffer, time);
  }

  captured_frames_count_ += audio_source->frames();
  if (VLOG_IS_ON(1)) {
    auto now = base::TimeTicks::Now();
    if ((now - last_frame_count_report_time_) >
        base::TimeDelta::FromMinutes(2)) {
      VLOG(1) << open_audio_stream_->device_id()
              << " captured frames: " << captured_frames_count_;
      last_frame_count_report_time_ = now;
    }
  }
}

// Runs on audio service thread.
void AudioInputImpl::OnCaptureError(const std::string& message) {
  LOG(ERROR) << open_audio_stream_->device_id() << " capture error " << message;
  base::AutoLock lock(lock_);
  for (auto* observer : observers_)
    observer->OnAudioError(AudioInput::Error::FATAL_ERROR);
}

// Runs on audio service thread.
void AudioInputImpl::OnCaptureMuted(bool is_muted) {}

// Run on LibAssistant thread.
assistant_client::BufferFormat AudioInputImpl::GetFormat() const {
  return g_current_format;
}

// Run on LibAssistant thread.
void AudioInputImpl::AddObserver(
    assistant_client::AudioInput::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(observer_sequence_checker_);
  VLOG(1) << " add observer";

  bool have_first_observer = false;
  {
    base::AutoLock lock(lock_);
    observers_.push_back(observer);
    have_first_observer = observers_.size() == 1;
  }

  if (have_first_observer) {
    // Post to main thread runner to start audio recording. Assistant thread
    // does not have thread context defined in //base and will fail sequence
    // check in AudioCapturerSource::Start().
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&AudioInputImpl::UpdateRecordingState,
                                          weak_factory_.GetWeakPtr()));
  }
}

// Run on LibAssistant thread.
void AudioInputImpl::RemoveObserver(
    assistant_client::AudioInput::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(observer_sequence_checker_);
  if (open_audio_stream_)
    VLOG(1) << open_audio_stream_->device_id() << " remove observer";

  bool have_no_observer = false;
  {
    base::AutoLock lock(lock_);
    base::Erase(observers_, observer);
    have_no_observer = observers_.size() == 0;
  }

  if (have_no_observer) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&AudioInputImpl::UpdateRecordingState,
                                          weak_factory_.GetWeakPtr()));

    // Reset the sequence checker since assistant may call from different thread
    // after restart.
    DETACH_FROM_SEQUENCE(observer_sequence_checker_);
  }
}

void AudioInputImpl::SetMicState(bool mic_open) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (mic_open_ == mic_open)
    return;

  mic_open_ = mic_open;
  UpdateRecordingState();
}

void AudioInputImpl::OnConversationTurnStarted() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  state_manager_->OnConversationTurnStarted();
}

void AudioInputImpl::OnConversationTurnFinished() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  state_manager_->OnConversationTurnFinished();
}

void AudioInputImpl::OnHotwordEnabled(bool enable) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (hotword_enabled_ == enable)
    return;

  hotword_enabled_ = enable;
  UpdateRecordingState();
}

void AudioInputImpl::SetDeviceId(const std::string& device_id) {
  if (preferred_device_id_ == device_id)
    return;

  preferred_device_id_ = device_id;

  UpdateRecordingState();
  if (open_audio_stream_)
    state_manager_->RecreateAudioInputStream();
}

void AudioInputImpl::SetHotwordDeviceId(const std::string& device_id) {
  if (hotword_device_id_ == device_id)
    return;

  hotword_device_id_ = device_id;
  RecreateStateManager();
  if (open_audio_stream_)
    state_manager_->RecreateAudioInputStream();
}

void AudioInputImpl::OnLidStateChanged(LidState new_state) {
  // Lid switch event still gets fired during system suspend, which enables
  // us to stop DSP recording correctly when user closes lid after the device
  // goes to sleep.
  if (new_state != lid_state_) {
    lid_state_ = new_state;
    UpdateRecordingState();
  }
}

void AudioInputImpl::RecreateAudioInputStream(bool use_dsp) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  StopRecording();

  open_audio_stream_ = std::make_unique<AudioStream>(
      audio_stream_factory_delegate_, GetDeviceId(use_dsp),
      ShouldEnableDeadStreamDetection(use_dsp), GetFormat(),
      /*capture_callback=*/this);

  VLOG(1) << open_audio_stream_->device_id() << " start recording";
}

bool AudioInputImpl::IsHotwordAvailable() const {
  return features::IsDspHotwordEnabled() && !hotword_device_id_.empty();
}

bool AudioInputImpl::IsRecordingForTesting() const {
  return !!open_audio_stream_;
}

bool AudioInputImpl::IsUsingHotwordDeviceForTesting() const {
  return IsRecordingForTesting()  // IN-TEST
         && open_audio_stream_->device_id() == hotword_device_id_ &&
         IsHotwordAvailable();
}

base::Optional<std::string> AudioInputImpl::GetOpenDeviceIdForTesting() const {
  if (!open_audio_stream_)
    return base::nullopt;
  return open_audio_stream_->device_id();
}

base::Optional<bool> AudioInputImpl::IsUsingDeadStreamDetectionForTesting()
    const {
  if (!open_audio_stream_)
    return base::nullopt;
  return open_audio_stream_->has_dead_stream_detection();
}

void AudioInputImpl::StartRecording() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!open_audio_stream_);
  RecreateAudioInputStream(IsHotwordAvailable());
}

void AudioInputImpl::StopRecording() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (open_audio_stream_) {
    VLOG(1) << open_audio_stream_->device_id() << " stop recording";
    VLOG(1) << open_audio_stream_->device_id()
            << " ending captured frames: " << captured_frames_count_;
    open_audio_stream_.reset();
  }
}

void AudioInputImpl::UpdateRecordingState() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  bool has_observers = false;
  {
    base::AutoLock lock(lock_);
    has_observers = observers_.size() > 0;
  }

  bool is_lid_closed = (lid_state_ == LidState::kClosed);
  bool should_enable_hotword =
      hotword_enabled_ && (!preferred_device_id_.empty());
  bool should_start =
      !is_lid_closed && (should_enable_hotword || mic_open_) && has_observers;

  if (!open_audio_stream_ && should_start)
    StartRecording();
  else if (open_audio_stream_ && !should_start)
    StopRecording();
}

std::string AudioInputImpl::GetDeviceId(bool use_dsp) const {
  if (use_dsp && !hotword_device_id_.empty())
    return hotword_device_id_;
  else if (!preferred_device_id_.empty())
    return preferred_device_id_;
  else
    return media::AudioDeviceDescription::kDefaultDeviceId;
}

bool AudioInputImpl::ShouldEnableDeadStreamDetection(bool use_dsp) const {
  if (use_dsp && !hotword_device_id_.empty()) {
    // The DSP device won't provide data until it detects a hotword, so
    // we disable its dead stream detection.
    return false;
  }
  return true;
}

}  // namespace assistant
}  // namespace chromeos
