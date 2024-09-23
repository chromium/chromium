// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/audio/audio_input_impl.h"

#include <cstdint>
#include <optional>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_browser_delegate.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "chromeos/ash/services/libassistant/audio/audio_input_stream.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_sample_types.h"
#include "media/base/channel_layout.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "services/audio/public/cpp/device_factory.h"

namespace ash::libassistant {

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
  explicit DspHotwordStateManager(AudioInputImpl* input)
      : AudioInputImpl::HotwordStateManager(input) {}

  DspHotwordStateManager(const DspHotwordStateManager&) = delete;
  DspHotwordStateManager& operator=(const DspHotwordStateManager&) = delete;

  // HotwordStateManager overrides:
  void OnConversationTurnStarted() override {
    if (second_phase_timer_.IsRunning()) {
      DCHECK(stream_state_ == StreamState::HOTWORD);
      second_phase_timer_.Stop();
    } else {
      // Handles user click on mic button.
      input_->RecreateAudioInputStream(false /* use_dsp */);
    }
    stream_state_ = StreamState::NORMAL;
  }

  void OnConversationTurnFinished() override {
    if (input_->IsHotwordEnabled()) {
      input_->RecreateAudioInputStream(true /* use_dsp */);
      if (stream_state_ == StreamState::HOTWORD) {
        // If |stream_state_| remains unchanged, that indicates the first stage
        // DSP hotword detection was rejected by Libassistant.
        RecordDspHotwordDetection(DspHotwordDetectionStatus::SOFTWARE_REJECTED);
      }
    }
    stream_state_ = StreamState::HOTWORD;
  }

  void OnCaptureDataArrived() override {
    if (stream_state_ == StreamState::HOTWORD &&
        !second_phase_timer_.IsRunning()) {
      RecordDspHotwordDetection(DspHotwordDetectionStatus::HARDWARE_ACCEPTED);
      // 1s from now, if OnConversationTurnStarted is not called, we assume that
      // libassistant has rejected the hotword supplied by DSP. Thus, we reset
      // and reopen the device on hotword state.
      second_phase_timer_.Start(
          FROM_HERE, base::Seconds(1),
          base::BindRepeating(
              &DspHotwordStateManager::OnConversationTurnFinished,
              base::Unretained(this)));
    }
  }

  void RecreateAudioInputStream() override {
    input_->RecreateAudioInputStream(stream_state_ == StreamState::HOTWORD);
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

  StreamState stream_state_ = StreamState::HOTWORD;
  base::OneShotTimer second_phase_timer_;
};

class AudioInputBufferImpl : public assistant_client::AudioBuffer {
 public:
  AudioInputBufferImpl(std::vector<int16_t>&& data, uint32_t frame_count)
      : data_(std::move(data)), frame_count_(frame_count) {}
  AudioInputBufferImpl(const AudioInputBufferImpl&) = delete;
  AudioInputBufferImpl& operator=(const AudioInputBufferImpl&) = delete;
  AudioInputBufferImpl(AudioInputBufferImpl&&) = default;
  AudioInputBufferImpl& operator=(AudioInputBufferImpl&&) = default;
  ~AudioInputBufferImpl() override = default;

  // assistant_client::AudioBuffer overrides:
  assistant_client::BufferFormat GetFormat() const override {
    return g_current_format;
  }
  const void* GetData() const override { return data_.data(); }
  void* GetWritableData() override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }
  int GetFrameCount() const override { return frame_count_; }

 private:
  std::vector<int16_t> data_;
  int frame_count_;
};

AudioInputBufferImpl ToAudioInputBuffer(const media::AudioBus* audio_source) {
  std::vector<int16_t> buffer(audio_source->channels() *
                              audio_source->frames());
  audio_source->ToInterleaved<media::SignedInt16SampleTypeTraits>(
      audio_source->frames(), buffer.data());
  return AudioInputBufferImpl(std::move(buffer), audio_source->frames());
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AudioCapturer
////////////////////////////////////////////////////////////////////////////////

// Helper class that will receive the callbacks from the audio source,
// and forward the audio data to Libassistant.
// Note that all callback methods in this object run on the audio service
// thread, so this class should be treated carefully.
// All public methods can be called from other threads, and the
// |on_capture_callback| will be invoked on the given callback thread.
class AudioCapturer : public media::AudioCapturerSource::CaptureCallback {
 public:
  explicit AudioCapturer(
      base::RepeatingCallback<void()> on_capture_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner)
      : on_capture_callback_(on_capture_callback),
        callback_task_runner_(callback_task_runner) {}
  AudioCapturer(const AudioCapturer&) = delete;
  AudioCapturer& operator=(const AudioCapturer&) = delete;
  ~AudioCapturer() override = default;

  void AddObserver(assistant_client::AudioInput::Observer* observer) {
    base::AutoLock lock(observers_lock_);
    observers_.push_back(observer);
  }

  void RemoveObserver(assistant_client::AudioInput::Observer* observer) {
    base::AutoLock lock(observers_lock_);
    std::erase(observers_, observer);
  }

  int num_observers() {
    base::AutoLock lock(observers_lock_);
    return observers_.size();
  }

  int captured_frames_count() { return captured_frames_count_; }

 private:
  // media::AudioCapturerSource::CaptureCallback implementation:
  // Runs on audio service thread.
  void Capture(const media::AudioBus* audio_source,
               base::TimeTicks audio_capture_time,
               const media::AudioGlitchInfo& glitch_info,
               double volume,
               bool key_pressed) override {
    DCHECK_EQ(g_current_format.num_channels, audio_source->channels());

    callback_task_runner_->PostTask(FROM_HERE, on_capture_callback_);

    UpdateCapturedFramesCount(audio_source->frames());

    AudioInputBufferImpl input_buffer(ToAudioInputBuffer(audio_source));
    int64_t time = ToLibassistantTime(audio_capture_time);

    base::AutoLock lock(observers_lock_);
    for (auto* observer : observers_)
      observer->OnAudioBufferAvailable(input_buffer, time);
  }

  // Runs on audio service thread.
  void OnCaptureError(media::AudioCapturerSource::ErrorCode code,
                      const std::string& message) override {
    LOG(ERROR) << "Capture error " << message
               << ", code=" << static_cast<uint32_t>(code);
    base::AutoLock lock(observers_lock_);
    for (auto* observer : observers_)
      observer->OnAudioError(assistant_client::AudioInput::Error::FATAL_ERROR);
  }

  // Runs on audio service thread.
  void OnCaptureMuted(bool is_muted) override {}

  int64_t ToLibassistantTime(base::TimeTicks audio_capture_time) const {
    // Only provide accurate timestamp when eraser is enabled, otherwise it
    // seems break normal libassistant voice recognition.
    if (assistant::features::IsAudioEraserEnabled())
      return audio_capture_time.since_origin().InMicroseconds();
    return 0;
  }

  void UpdateCapturedFramesCount(int num_arrived_frames) {
    captured_frames_count_ += num_arrived_frames;
    if (VLOG_IS_ON(1)) {
      auto now = base::TimeTicks::Now();
      if ((now - last_frame_count_report_time_) > base::Minutes(2)) {
        VLOG(1) << "Captured frames: " << captured_frames_count_;
        last_frame_count_report_time_ = now;
      }
    }
  }

  // This is the total number of frames captured during the life time of this
  // object. We don't worry about overflow because this count is only used for
  // logging purposes. If in the future this changes, we should re-evaluate.
  int captured_frames_count_ = 0;
  base::TimeTicks last_frame_count_report_time_;

  base::Lock observers_lock_;
  std::vector<assistant_client::AudioInput::Observer*> observers_
      GUARDED_BY(observers_lock_);

  // |on_capture_callback| must always be called from the main thread.
  base::RepeatingCallback<void()> on_capture_callback_;
  scoped_refptr<base::SequencedTaskRunner> callback_task_runner_;
};

////////////////////////////////////////////////////////////////////////////////
// AudioInputImpl
////////////////////////////////////////////////////////////////////////////////

AudioInputImpl::HotwordStateManager::HotwordStateManager(
    AudioInputImpl* audio_input)
    : input_(audio_input) {}

void AudioInputImpl::HotwordStateManager::RecreateAudioInputStream() {
  input_->RecreateAudioInputStream(/*use_dsp=*/false);
}

AudioInputImpl::AudioInputImpl(const std::optional<std::string>& device_id)
    : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      preferred_device_id_(device_id),
      weak_factory_(this) {
  DETACH_FROM_SEQUENCE(observer_sequence_checker_);

  audio_capturer_ = std::make_unique<AudioCapturer>(
      base::BindRepeating(&AudioInputImpl::OnCaptureDataArrived,
                          weak_factory_.GetWeakPtr()),
      /*callback_task_runner=*/base::SequencedTaskRunner::GetCurrentDefault());

  RecreateStateManager();
  if (assistant::features::IsStereoAudioInputEnabled())
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
    state_manager_ = std::make_unique<DspHotwordStateManager>(this);
  } else {
    state_manager_ = std::make_unique<HotwordStateManager>(this);
  }
}

void AudioInputImpl::OnCaptureDataArrived() {
  state_manager_->OnCaptureDataArrived();
}

void AudioInputImpl::Initialize(mojom::PlatformDelegate* platform_delegate) {
  platform_delegate_ = platform_delegate;
  DCHECK(platform_delegate_);
  UpdateRecordingState();
}

// Run on LibAssistant thread.
assistant_client::BufferFormat AudioInputImpl::GetFormat() const {
  return g_current_format;
}

// Run on LibAssistant thread.
void AudioInputImpl::AddObserver(
    assistant_client::AudioInput::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(observer_sequence_checker_);
  VLOG(1) << "Add observer";

  audio_capturer_->AddObserver(observer);

  if (audio_capturer_->num_observers() == 1) {
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
  VLOG(1) << "Remove observer";

  audio_capturer_->RemoveObserver(observer);

  if (audio_capturer_->num_observers() == 0) {
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

void AudioInputImpl::SetDeviceId(const std::optional<std::string>& device_id) {
  DVLOG(1) << "Set audio input preferred_device_id to "
           << device_id.value_or("<null>");
  auto new_device_id = device_id;

  constexpr char kAssistantForceDefaultAudioInput[] =
      "assistant-force-default-audio-input";
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kAssistantForceDefaultAudioInput)) {
    // Sometimes there may not be a preferred audio device,
    // e.g. if the device does not have built-in mic and using a bluetooth
    // microphone, in this case we do not want to open the bluetooth device by
    // default to drain the battery; also if running linux chromeos chrome
    // build, there won't be cras and we won't have a device id set. Force using
    // default audio input in these cases to mimic the common Assistant hotword
    // behaviors.
    DVLOG(1) << "Force audio input preferred_device_id to default.";
    new_device_id = media::AudioDeviceDescription::kDefaultDeviceId;
  }

  if (preferred_device_id_ == new_device_id)
    return;

  preferred_device_id_ = new_device_id;

  UpdateRecordingState();
  if (HasOpenAudioStream())
    state_manager_->RecreateAudioInputStream();
}

void AudioInputImpl::SetHotwordDeviceId(
    const std::optional<std::string>& device_id) {
  if (hotword_device_id_ == device_id)
    return;

  hotword_device_id_ = device_id;
  RecreateStateManager();
  if (HasOpenAudioStream())
    state_manager_->RecreateAudioInputStream();
}

void AudioInputImpl::OnLidStateChanged(mojom::LidState new_state) {
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

  open_audio_stream_ = std::make_unique<AudioInputStream>(
      platform_delegate_, GetDeviceId(use_dsp),
      ShouldEnableDeadStreamDetection(use_dsp), GetFormat(),
      /*capture_callback=*/audio_capturer_.get());

  VLOG(1) << open_audio_stream_->device_id() << " start recording";
}

bool AudioInputImpl::IsHotwordAvailable() const {
  return assistant::features::IsDspHotwordEnabled() &&
         hotword_device_id_.has_value();
}

bool AudioInputImpl::IsRecordingForTesting() const {
  return HasOpenAudioStream();
}

bool AudioInputImpl::IsUsingHotwordDeviceForTesting() const {
  return IsRecordingForTesting()  // IN-TEST
         && GetOpenDeviceId() == hotword_device_id_ && IsHotwordAvailable();
}

bool AudioInputImpl::IsMicOpenForTesting() const {
  return mic_open_;
}

std::optional<std::string> AudioInputImpl::GetOpenDeviceIdForTesting() const {
  return GetOpenDeviceId();
}

std::optional<bool> AudioInputImpl::IsUsingDeadStreamDetectionForTesting()
    const {
  if (!open_audio_stream_)
    return std::nullopt;
  return open_audio_stream_->has_dead_stream_detection();
}

void AudioInputImpl::OnCaptureDataArrivedForTesting() {
  OnCaptureDataArrived();
}

void AudioInputImpl::StartRecording() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!HasOpenAudioStream());
  RecreateAudioInputStream(IsHotwordAvailable() && IsHotwordEnabled());
}

void AudioInputImpl::StopRecording() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (open_audio_stream_) {
    VLOG(1) << open_audio_stream_->device_id() << " stop recording";
    VLOG(1) << open_audio_stream_->device_id() << " ending captured frames: "
            << audio_capturer_->captured_frames_count();
    open_audio_stream_.reset();
  }
}

void AudioInputImpl::UpdateRecordingState() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  bool has_observers = (audio_capturer_->num_observers() > 0);
  bool is_lid_closed = (lid_state_ == mojom::LidState::kClosed);
  bool should_enable_hotword =
      hotword_enabled_ && preferred_device_id_.has_value();
  bool has_delegate = (platform_delegate_ != nullptr);
  bool should_start = !is_lid_closed && (should_enable_hotword || mic_open_) &&
                      has_observers && has_delegate;

  VLOG(1) << "UpdateRecordingState: "
          << "    is_lid_closed: " << is_lid_closed << "\n"
          << "    hotword_enabled: " << hotword_enabled_ << "\n"
          << "    preferred_device_id: '"
          << preferred_device_id_.value_or("<unset>") << "'\n"
          << "    hotword_device_id: '"
          << hotword_device_id_.value_or("<unset>") << "'\n"
          << "    mic_open: " << mic_open_ << "\n"
          << "    has_observers: " << has_observers << "\n"
          << "    has_delegate: " << has_delegate << "\n"
          << " => should_start: " << should_start;

  if (!HasOpenAudioStream() && should_start)
    StartRecording();
  else if (HasOpenAudioStream() && !should_start)
    StopRecording();
}

std::string AudioInputImpl::GetDeviceId(bool use_dsp) const {
  if (use_dsp && hotword_device_id_.has_value())
    return hotword_device_id_.value();
  else if (preferred_device_id_.has_value())
    return preferred_device_id_.value();
  else
    return media::AudioDeviceDescription::kDefaultDeviceId;
}

std::optional<std::string> AudioInputImpl::GetOpenDeviceId() const {
  if (!open_audio_stream_)
    return std::nullopt;
  return open_audio_stream_->device_id();
}

bool AudioInputImpl::ShouldEnableDeadStreamDetection(bool use_dsp) const {
  if (use_dsp && hotword_device_id_.has_value()) {
    // The DSP device won't provide data until it detects a hotword, so
    // we disable its the dead stream detection.
    return false;
  }
  return true;
}

bool AudioInputImpl::HasOpenAudioStream() const {
  return open_audio_stream_ != nullptr;
}

}  // namespace ash::libassistant
