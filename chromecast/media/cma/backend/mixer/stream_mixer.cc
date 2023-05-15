// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer/stream_mixer.h"

#include <pthread.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/synchronization/condition_variable.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/base/thread_health_checker.h"
#include "chromecast/media/audio/audio_io_thread.h"
#include "chromecast/media/audio/audio_log.h"
#include "chromecast/media/audio/interleaved_channel_mixer.h"
#include "chromecast/media/audio/mixer_service/loopback_interrupt_reason.h"
#include "chromecast/media/base/audio_device_ids.h"
#include "chromecast/media/cma/backend/cast_audio_json.h"
#include "chromecast/media/cma/backend/mixer/audio_output_redirector.h"
#include "chromecast/media/cma/backend/mixer/channel_layout.h"
#include "chromecast/media/cma/backend/mixer/filter_group.h"
#include "chromecast/media/cma/backend/mixer/loopback_handler.h"
#include "chromecast/media/cma/backend/mixer/mixer_service_receiver.h"
#include "chromecast/media/cma/backend/mixer/post_processing_pipeline_impl.h"
#include "chromecast/media/cma/backend/mixer/post_processing_pipeline_parser.h"
#include "chromecast/media/cma/backend/volume_map.h"
#include "chromecast/public/media/mixer_output_stream.h"
#include "media/audio/audio_device_description.h"

#define RUN_ON_MIXER_THREAD(method, ...)                                   \
  do {                                                                     \
    mixer_task_runner_->PostTask(                                          \
        FROM_HERE, base::BindOnce(&StreamMixer::method,                    \
                                  base::Unretained(this), ##__VA_ARGS__)); \
  } while (0)

namespace chromecast {
namespace media {

namespace {

const size_t kMinInputChannels = 2;
const int kDefaultInputChannels = 2;
const int kInvalidNumChannels = 0;

const int kDefaultCheckCloseTimeoutMs = 2000;

// Resample all audio below this frequency.
const unsigned int kLowSampleRateCutoff = 32000;

// Sample rate to fall back if input sample rate is below kLowSampleRateCutoff.
const unsigned int kLowSampleRateFallback = 48000;

const int64_t kNoTimestamp = std::numeric_limits<int64_t>::min();

const int kUseDefaultFade = -1;
const int kMediaDuckFadeMs = 150;
const int kMediaUnduckFadeMs = 700;
const int kDefaultFilterFrameAlignment = 64;

constexpr base::TimeDelta kMixerThreadCheckTimeout = base::Seconds(10);
constexpr base::TimeDelta kHealthCheckInterval = base::Seconds(5);

int GetFixedOutputSampleRate() {
  int fixed_sample_rate = GetSwitchValueNonNegativeInt(
      switches::kAudioOutputSampleRate, MixerOutputStream::kInvalidSampleRate);

  if (fixed_sample_rate == MixerOutputStream::kInvalidSampleRate) {
    fixed_sample_rate =
        GetSwitchValueNonNegativeInt(switches::kAlsaFixedOutputSampleRate,
                                     MixerOutputStream::kInvalidSampleRate);
  }
  return fixed_sample_rate;
}

base::TimeDelta GetNoInputCloseTimeout() {
  // --accept-resource-provider should imply a check close timeout of 0.
  int default_close_timeout_ms =
      GetSwitchValueBoolean(switches::kAcceptResourceProvider, false)
          ? 0
          : kDefaultCheckCloseTimeoutMs;
  int close_timeout_ms = GetSwitchValueInt(switches::kAlsaCheckCloseTimeout,
                                           default_close_timeout_ms);
  if (close_timeout_ms < 0) {
    return base::TimeDelta::Max();
  }
  return base::Milliseconds(close_timeout_ms);
}

void UseHighPriority() {
#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_ANDROID)
  struct sched_param params;
  params.sched_priority = sched_get_priority_max(SCHED_FIFO);
  pthread_setschedparam(pthread_self(), SCHED_FIFO, &params);

  int policy = 0;
  struct sched_param actual_params;
  pthread_getschedparam(pthread_self(), &policy, &actual_params);
  LOG(INFO) << "Actual priority = " << actual_params.sched_priority
            << ", policy = " << policy;
#endif
}

const char* ChannelString(int num_channels) {
  if (num_channels == 1) {
    return "channel";
  }
  return "channels";
}

}  // namespace

class StreamMixer::ExternalMediaVolumeChangeRequestObserver
    : public StreamMixer::BaseExternalMediaVolumeChangeRequestObserver {
 public:
  explicit ExternalMediaVolumeChangeRequestObserver(StreamMixer* mixer)
      : mixer_(mixer) {
    DCHECK(mixer_);
  }

  // ExternalAudioPipelineShlib::ExternalMediaVolumeChangeRequestObserver
  // implementation:
  void OnVolumeChangeRequest(float new_volume) override {
    mixer_->SetVolume(AudioContentType::kMedia, new_volume);
  }

  void OnMuteChangeRequest(bool new_muted) override {
    mixer_->SetMuted(AudioContentType::kMedia, new_muted);
  }

 private:
  StreamMixer* const mixer_;
};

class StreamMixer::MixerThread : public base::PlatformThread::Delegate,
                                 public base::TaskRunner {
 public:
  MixerThread() : cond_(&lock_) {
    tasks_.reserve(64);
    swapped_tasks_.reserve(64);

    CHECK(base::PlatformThread::CreateWithType(
        256 * 1024, this, &thread_, base::ThreadType::kRealtimeAudio));
  }

  void Stop() {
    {
      base::AutoLock lock(lock_);
      if (stopped_) {
        return;
      }
      stopped_ = true;
    }
    cond_.Signal();

    base::PlatformThread::Join(thread_);
  }

  // base::TaskRunner implementation:
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    // Delay is ignored.
    DCHECK_EQ(delay, base::TimeDelta());
    {
      base::AutoLock lock(lock_);
      if (stopped_) {
        return false;
      }
      tasks_.push_back(std::move(task));
    }
    cond_.Signal();
    return true;
  }

 private:
  friend class RefCountedThreadSafe<MixerThread>;

  ~MixerThread() override { Stop(); }

  // base::PlatformThread::Delegate implementation:
  void ThreadMain() override {
    base::PlatformThread::SetName("CMA mixer");
    UseHighPriority();

    base::AutoLock lock(lock_);
    for (;;) {
      swapped_tasks_.swap(tasks_);
      if (swapped_tasks_.empty()) {
        if (stopped_) {
          return;
        }
        cond_.Wait();
      } else {
        base::AutoUnlock unlock(lock_);
        for (auto& task : swapped_tasks_) {
          std::move(task).Run();
        }
        swapped_tasks_.clear();
      }
    }
  }

  base::Lock lock_;
  base::ConditionVariable cond_;
  bool stopped_ GUARDED_BY(lock_) = false;
  std::vector<base::OnceClosure> tasks_ GUARDED_BY(lock_);

  // Only used on the mixer thread.
  std::vector<base::OnceClosure> swapped_tasks_;

  base::PlatformThreadHandle thread_;
};

StreamMixer::StreamMixer(
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : StreamMixer(nullptr,
                  nullptr,
                  "",
                  std::move(io_task_runner)) {}

StreamMixer::StreamMixer(
    std::unique_ptr<MixerOutputStream> output,
    scoped_refptr<base::SequencedTaskRunner> mixer_task_runner,
    const std::string& pipeline_json,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : output_(std::move(output)),
      post_processing_pipeline_factory_(
          std::make_unique<PostProcessingPipelineFactoryImpl>()),
      mixer_task_runner_(mixer_task_runner),
      io_task_runner_(std::move(io_task_runner)),
      enable_dynamic_channel_count_(
          GetSwitchValueBoolean(switches::kMixerEnableDynamicChannelCount,
                                false)),
      low_sample_rate_cutoff_(
          GetSwitchValueBoolean(switches::kAlsaEnableUpsampling, false)
              ? kLowSampleRateCutoff
              : MixerOutputStream::kInvalidSampleRate),
      fixed_num_output_channels_(
          GetSwitchValueNonNegativeInt(switches::kAudioOutputChannels,
                                       kInvalidNumChannels)),
      fixed_output_sample_rate_(GetFixedOutputSampleRate()),
      no_input_close_timeout_(GetNoInputCloseTimeout()),
      filter_frame_alignment_(kDefaultFilterFrameAlignment),
      state_(kStateStopped),
      external_audio_pipeline_supported_(
          ExternalAudioPipelineShlib::IsSupported()),
      weak_factory_(this) {
  LOG(INFO) << __func__;
  DETACH_FROM_SEQUENCE(mixer_sequence_checker_);
  logging::InitializeAudioLog();

  volume_info_[AudioContentType::kOther].volume = 1.0f;
  volume_info_[AudioContentType::kOther].limit = 1.0f;
  volume_info_[AudioContentType::kOther].muted = false;

  if (mixer_task_runner_) {
    // Test mode.
    if (!io_task_runner_) {
      io_task_runner_ = mixer_task_runner;
    }
  } else {
    mixer_thread_ = base::MakeRefCounted<MixerThread>();
    mixer_task_runner_ = mixer_thread_;

    if (!io_task_runner_) {
      io_task_runner_ = AudioIoThread::Get()->task_runner();
    }

    health_checker_ = std::make_unique<ThreadHealthChecker>(
        mixer_task_runner_, io_task_runner_, kHealthCheckInterval,
        kMixerThreadCheckTimeout,
        base::BindRepeating(&StreamMixer::OnHealthCheckFailed,
                            base::Unretained(this)));
    LOG(INFO) << "Mixer health checker started";
  }

  if (fixed_output_sample_rate_ != MixerOutputStream::kInvalidSampleRate) {
    LOG(INFO) << "Setting fixed sample rate to " << fixed_output_sample_rate_;
  }

  {
    base::AutoLock lock(input_creation_lock_);
    CreatePostProcessors([](bool, const std::string&) {}, pipeline_json,
                         kDefaultInputChannels);
  }

  // TODO(jyw): command line flag for filter frame alignment.
  DCHECK_EQ(filter_frame_alignment_ & (filter_frame_alignment_ - 1), 0)
      << "Alignment must be a power of 2.";

  if (external_audio_pipeline_supported_) {
    external_volume_observer_ =
        std::make_unique<ExternalMediaVolumeChangeRequestObserver>(this);
    ExternalAudioPipelineShlib::AddExternalMediaVolumeChangeRequestObserver(
        external_volume_observer_.get());
  }

  loopback_handler_ = std::make_unique<LoopbackHandler>(io_task_runner_);
  receiver_ = base::SequenceBound<MixerServiceReceiver>(
      io_task_runner_, this, loopback_handler_.get());
  UpdateStreamCounts();
}

void StreamMixer::OnHealthCheckFailed() {
  LOG(FATAL) << "Crash on mixer thread health check failure!";
}

void StreamMixer::ResetPostProcessors(CastMediaShlib::ResultCallback callback) {
  VolumeMap::Reload();
  RUN_ON_MIXER_THREAD(ResetPostProcessorsOnThread, std::move(callback), "");
}

void StreamMixer::ResetPostProcessorsOnThread(
    CastMediaShlib::ResultCallback callback,
    const std::string& override_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mixer_sequence_checker_);

  {
    base::AutoLock lock(input_creation_lock_);
    // Detach inputs.
    for (const auto& input : inputs_) {
      input.second->SetFilterGroup(nullptr);
    }

    int expected_input_channels = kDefaultInputChannels;
    for (const auto& input : inputs_) {
      if (input.second->primary()) {
        expected_input_channels =
            std::max(expected_input_channels, input.second->num_channels());
      }
    }
    CreatePostProcessors(std::move(callback), override_config,
                         expected_input_channels);
  }

  // Re-attach inputs.
  for (const auto& input : inputs_) {
    FilterGroup* input_group =
        mixer_pipeline_->GetInputGroup(input.first->device_id());
    DCHECK(input_group) << "No input group for input.first->device_id()";
    input.second->SetFilterGroup(input_group);
  }
}

// May be called on mixer_task_runner_ or from ctor
void StreamMixer::CreatePostProcessors(CastMediaShlib::ResultCallback callback,
                                       const std::string& override_config,
                                       int expected_input_channels) {
  // (Re)-create post processors.
  mixer_pipeline_.reset();

  if (!override_config.empty()) {
    auto value = base::JSONReader::Read(override_config);
    CHECK(value) << "Invalid JSON";
    PostProcessingPipelineParser parser(std::move(*value));
    mixer_pipeline_ = MixerPipeline::CreateMixerPipeline(
        &parser, post_processing_pipeline_factory_.get(),
        expected_input_channels);
  } else {
    PostProcessingPipelineParser parser(CastAudioJson::GetFilePath());
    mixer_pipeline_ = MixerPipeline::CreateMixerPipeline(
        &parser, post_processing_pipeline_factory_.get(),
        expected_input_channels);
  }

  // Attempt to fall back to built-in cast_audio.json, unless we were reset with
  // an override config.
  if (!mixer_pipeline_ && override_config.empty()) {
    AUDIO_LOG(WARNING) << "Invalid cast_audio.json config loaded. Retrying with"
                          " read-only config";
    callback(false,
             "Unable to build pipeline.");  // TODO(bshaya): Send more specific
                                            // error message.
    callback = nullptr;
    PostProcessingPipelineParser parser(CastAudioJson::GetReadOnlyFilePath());
    mixer_pipeline_.reset();
    mixer_pipeline_ = MixerPipeline::CreateMixerPipeline(
        &parser, post_processing_pipeline_factory_.get(),
        expected_input_channels);
  }

  CHECK(mixer_pipeline_) << "Unable to load post processor config!";
  if (fixed_num_output_channels_ != kInvalidNumChannels &&
      fixed_num_output_channels_ != mixer_pipeline_->GetOutputChannelCount()) {
    // Just log a warning, but this is still fine because we will remap the
    // channels prior to output.
    AUDIO_LOG(WARNING) << "PostProcessor configuration output channel count"
                       << " does not match command line flag: "
                       << mixer_pipeline_->GetOutputChannelCount() << " vs "
                       << fixed_num_output_channels_
                       << ". Channels will be remapped";
  }

  if (state_ == kStateRunning) {
    mixer_pipeline_->Initialize(output_samples_per_second_, frames_per_write_);
  }

  post_processor_input_channels_ = expected_input_channels;

  if (callback) {
    callback(true, "");
  }
}

void StreamMixer::ResetPostProcessorsForTest(
    std::unique_ptr<PostProcessingPipelineFactory> pipeline_factory,
    const std::string& pipeline_json) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mixer_sequence_checker_);
  LOG(INFO) << __FUNCTION__ << " disregard previous PostProcessor messages.";
  mixer_pipeline_.reset();
  post_processing_pipeline_factory_ = std::move(pipeline_factory);
  ResetPostProcessorsOnThread([](bool, const std::string&) {}, pipeline_json);
}

void StreamMixer::SetNumOutputChannelsForTest(int num_output_channels) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mixer_sequence_checker_);
  fixed_num_output_channels_ = num_output_channels;
}

void StreamMixer::EnableDynamicChannelCountForTest(bool enable) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mixer_sequence_checker_);
  enable_dynamic_channel_count_ = enable;
}

LoopbackHandler* StreamMixer::GetLoopbackHandlerForTest() {
  return loopback_handler_.get();
}

StreamMixer::~StreamMixer() {
  LOG(INFO) << __func__;

  receiver_.Reset();

  mixer_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&StreamMixer::FinalizeOnMixerThread,
                                base::Unretained(this)));
  if (mixer_thread_) {
    mixer_thread_->Stop();
  }

  if (external_volume_observer_) {
    ExternalAudioPipelineShlib::RemoveExternalMediaVolumeChangeRequestObserver(
        external_volume_observer_.get());
  }
}

void StreamMixer::FinalizeOnMixerThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mixer_sequence_checker_);
  {
    base::AutoLock lock(input_creation_lock_);
    Stop(LoopbackInterruptReason::kOutputStopped);
  }

  inputs_.clear();
  ignored_inputs_.clear();
}

void StreamMixer::SetNumOutputChannels(int num_channels) {
  RUN_ON_MIXER_THREAD(SetNumOutputChannelsOnThread, num_channels);
}

void StreamMixer::SetNumOutputChannelsOnThread(int num_channels) {
  AUDIO_LOG(INFO) << "Set the number of output channels to " << num_channels;
  enable_dynamic_channel_count_ = true;
  fixed_num_output_channels_ = num_channels;

  if (state_ == kStateRunning && num_channels != num_output_channels_) {
    {
      base::AutoLock lock(input_creation_lock_);
      Stop(LoopbackInterruptReason::kConfigChange);
    }
    Start();
  }
}

void StreamMixer::Start() {
  AUDIO_LOG(INFO) << __func__ << " with " << inputs_.size() << " active inputs";
  DCHECK_CALLED_ON_VALID_SEQUENCE(mixer_sequence_checker_);
  DCHECK(state_ == kStateStopped);

  {
    base::AutoLock lock(input_creation_lock_);
    // Detach inputs.
    for (const auto& input : inputs_) {
      input.second->SetFilterGroup(nullptr);
    }

    if (post_processor_input_channels_ != requested_input_channels_) {
      CreatePostProcessors([](bool, const std::string&) {},
                           "" /* override_config */, requested_input_channels_);
    }

    if (!output_) {
      if (external_audio_pipeline_supported_) {
        output_ = ExternalAudioPipelineShlib::CreateMixerOutputStream();
      } else {
        output_ = MixerOutputStream::Create();
      }
    }
    DCHECK(output_);

    int requested_output_channels;
    if (fixed_num_output_channels_ != kInvalidNumChannels) {
      requested_output_channels = fixed_num_output_channels_;
    } else {
      requested_output_channels = mixer_pipeline_->GetOutputChannelCount();
    }

    int requested_sample_rate;
    if (fixed_output_sample_rate_ != MixerOutputStream::kInvalidSampleRate) {
      requested_sample_rate = fixed_output_sample_rate_;
    } else if (low_sample_rate_cutoff_ !=
                   MixerOutputStream::kInvalidSampleRate &&
               requested_output_samples_per_second_ < low_sample_rate_cutoff_) {
      requested_sample_rate =
          output_samples_per_second_ != MixerOutputStream::kInvalidSampleRate
              ? output_samples_per_second_
              : kLowSampleRateFallback;
    } else {
      requested_sample_rate = requested_output_samples_per_second_;
    }

    if (!output_->Start(requested_sample_rate, requested_output_channels)) {
      Stop(LoopbackInterruptReason::kOutputStopped);
      return;
    }

    num_output_channels_ = output_->GetNumChannels();
    output_samples_per_second_ = output_->GetSampleRate();
    AUDIO_LOG(INFO) << "Output " << num_output_channels_ << " "
                    << ChannelString(num_output_channels_) << " at "
                    << output_samples_per_second_ << " samples per second";
    // Make sure the number of frames meets the filter alignment requirements.
    frames_per_write_ =
        output_->OptimalWriteFramesCount() & ~(filter_frame_alignment_ - 1);
    CHECK_GT(frames_per_write_, 0);

    output_channel_mixer_ = std::make_unique<InterleavedChannelMixer>(
        mixer::GuessChannelLayout(mixer_pipeline_->GetOutputChannelCount()),
        mixer_pipeline_->GetOutputChannelCount(),
        mixer::GuessChannelLayout(num_output_channels_), num_output_channels_,
        frames_per_write_);

    int num_loopback_channels = mixer_pipeline_->GetLoopbackChannelCount();
    if (!enable_dynamic_channel_count_ && num_output_channels_ == 1) {
      num_loopback_channels = 1;
    }
    AUDIO_LOG(INFO) << "Using " << num_loopback_channels << " loopback "
                    << ChannelString(num_loopback_channels);
    loopback_channel_mixer_ = std::make_unique<InterleavedChannelMixer>(
        mixer::GuessChannelLayout(mixer_pipeline_->GetLoopbackChannelCount()),
        mixer_pipeline_->GetLoopbackChannelCount(),
        mixer::GuessChannelLayout(num_loopback_channels), num_loopback_channels,
        frames_per_write_);

    loopback_handler_->SetDataSize(frames_per_write_ *
                                   mixer_pipeline_->GetLoopbackChannelCount() *
                                   sizeof(float));

    // Initialize filters.
    mixer_pipeline_->Initialize(output_samples_per_second_, frames_per_write_);

    // Determine the appropriate sample rate for the redirector. If a product
    // needs to have these be different and support redirecting, then we will
    // need to add/update the per-input resamplers before redirecting.
    redirector_samples_per_second_ = GetSampleRateForDeviceId(
        ::media::AudioDeviceDescription::kDefaultDeviceId);

    const std::vector<const char*> redirectable_device_ids = {
        kPlatformAudioDeviceId, kAlarmAudioDeviceId, kTtsAudioDeviceId,
        ::media::AudioDeviceDescription::kDefaultDeviceId,
        ::media::AudioDeviceDescription::kCommunicationsDeviceId};

    for (const char* device_id : redirectable_device_ids) {
      DCHECK_EQ(redirector_samples_per_second_,
                GetSampleRateForDeviceId(device_id));
    }

    redirector_frames_per_write_ = redirector_samples_per_second_ *
                                   frames_per_write_ /
                                   output_samples_per_second_;
    for (auto& redirector : audio_output_redirectors_) {
      redirector.second->SetSampleRate(redirector_samples_per_second_);
    }

    state_ = kStateRunning;
    playback_loop_task_ = base::BindRepeating(&StreamMixer::PlaybackLoop,
                                              weak_factory_.GetWeakPtr());

    // Write one buffer of silence to get correct rendering delay in the
    // postprocessors.
    WriteOneBuffer();
  }

  // Re-attach inputs.
  for (const auto& input : inputs_) {
    FilterGroup* input_group =
        mixer_pipeline_->GetInputGroup(input.first->device_id());
    DCHECK(input_group) << "No input group for input.first->device_id()";
    input.second->SetFilterGroup(input_group);
  }

  mixer_task_runner_->PostTask(FROM_HERE, playback_loop_task_);
}

void StreamMixer::Stop(LoopbackInterruptReason reason) {
  AUDIO_LOG(INFO) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(mixer_sequence_checker_);

  weak_factory_.InvalidateWeakPtrs();
  loopback_handler_->SendInterrupt(reason);

  if (output_) {
    output_->Stop();
  }

  state_ = kStateStopped;
  output_samples_per_second_ = MixerOutputStream::kInvalidSampleRate;
}

void StreamMixer::CheckChangeOutputParams(int num_input_channels,
                                          int input_samples_per_second) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mixer_sequence_checker_);
  if (state_ != kStateRunning) {
    return;
  }

  bool num_input_channels_unchanged =
      (num_input_channels == post_processor_input_channels_);

  bool sample_rate_unchanged =
      (fixed_output_sample_rate_ != MixerOutputStream::kInvalidSampleRate ||
       input_samples_per_second == requested_output_samples_per_second_ ||
       input_samples_per_second == output_samples_per_second_ ||
       input_samples_per_second < static_cast<int>(low_sample_rate_cutoff_));

  if (num_input_channels_unchanged && sample_rate_unchanged) {
    return;
  }

  for (const auto& input : inputs_) {
    if (input.second->primary()) {
      return;
    }
  }

  // Ignore existing inputs.
  SignalError(MixerInput::Source::MixerError::kInputIgnored);

  requested_input_channels_ = num_input_channels;
  requested_output_samples_per_second_ = input_samples_per_second;

  // Restart the output so that the new output params take effect.
  {
    base::AutoLock lock(input_creation_lock_);
    Stop(LoopbackInterruptReason::kConfigChange);
  }
  Start();
}

void StreamMixer::SignalError(MixerInput::Source::MixerError error) {
  // Move all current inputs to the ignored list and inform them of the error.
  for (auto& input : inputs_) {
    input.second->SignalError(error);
    ignored_inputs_.insert(std::move(input));
  }
  inputs_.clear();
  SetCloseTimeout();
  UpdateStreamCountsOnThread();
}

int StreamMixer::GetEffectiveChannelCount(MixerInput::Source* input_source) {
  AUDIO_LOG(INFO) << "Input source channel count = "
                  << input_source->num_channels();
  if (!enable_dynamic_channel_count_) {
    AUDIO_LOG(INFO) << "Dynamic channel count not enabled; using stereo";
    return kDefaultInputChannels;
  }

  // Most streams are at least stereo; to avoid unnecessary pipeline
  // reconfiguration, treat mono streams as stereo when calculating pipeline
  // inputs channel count.
  return std::max(input_source->num_channels(), kMinInputChannels);
}

void StreamMixer::AddInput(MixerInput::Source* input_source) {
  std::unique_ptr<MixerInput> input;
  {
    base::AutoLock lock(input_creation_lock_);
    if (state_ == kStateRunning) {
      input = CreateInput(input_source);
    }
  }
  RUN_ON_MIXER_THREAD(AddInputOnThread, input_source, std::move(input));
}

std::unique_ptr<MixerInput> StreamMixer::CreateInput(
    MixerInput::Source* input_source) {
  FilterGroup* input_group =
      mixer_pipeline_->GetInputGroup(input_source->device_id());
  DCHECK(input_group) << "Could not find a processor for "
                      << input_source->device_id();

  AUDIO_LOG(INFO) << "Add input " << input_source << " to "
                  << input_group->name() << " @ "
                  << input_group->GetInputSampleRate()
                  << " samples per second. Is primary source? = "
                  << input_source->primary();

  return std::make_unique<MixerInput>(input_source, input_group);
}

void StreamMixer::AddInputOnThread(MixerInput::Source* input_source,
                                   std::unique_ptr<MixerInput> input) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mixer_sequence_checker_);
  DCHECK(input_source);

  // If the new input is a primary one (or there were no inputs previously), we
  // may need to change the output sample rate to match the input sample rate.
  // We only change the output rate if it is not set to a fixed value.
  if (input_source->primary() || inputs_.empty()) {
    CheckChangeOutputParams(GetEffectiveChannelCount(input_source),
                            input_source->sample_rate());
  }

  if (state_ == kStateStopped) {
    requested_input_channels_ = GetEffectiveChannelCount(input_source);
    requested_output_samples_per_second_ = input_source->sample_rate();
    Start();
  }

  if (!input) {
    input = CreateInput(input_source);
  } else {
    // Make sure the input is using the right filter group (since config could
    // have been changed since the input was created).
    FilterGroup* input_group =
        mixer_pipeline_->GetInputGroup(input_source->device_id());
    DCHECK(input_group) << "Could not find a processor for "
                        << input_source->device_id();
    input->SetFilterGroup(input_group);
  }
  DCHECK(input);
  input->Initialize();
  if (state_ != kStateRunning) {
    // Mixer error occurred, signal error.
    MixerInput* input_ptr = input.get();
    ignored_inputs_[input_source] = std::move(input);
    input_ptr->SignalError(MixerInput::Source::MixerError::kInternalError);
    return;
  }

  auto type = input->content_type();
  if (type != AudioContentType::kOther) {
    input->SetContentTypeVolume(volume_info_[type].volume);
    input->SetMuted(volume_info_[type].muted);
  }
  if (input->primary() && input->focus_type() != AudioContentType::kOther) {
    input->SetOutputLimit(volume_info_[input->focus_type()].limit,
                          kUseDefaultFade);
  }

  for (auto& redirector : audio_output_redirectors_) {
    redirector.second->AddInput(input.get());
  }

  inputs_[input_source] = std::move(input);
  UpdateStreamCountsOnThread();
}

void StreamMixer::RemoveInput(MixerInput::Source* input_source) {
  // Always post a task to avoid synchronous deletion.
  RUN_ON_MIXER_THREAD(RemoveInputOnThread, input_source);
}

void StreamMixer::RemoveInputOnThread(MixerInput::Source* input_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mixer_sequence_checker_);
  DCHECK(input_source);

  AUDIO_LOG(INFO) << "Remove input " << input_source;

  std::unique_ptr<MixerInput> input;
  auto it = inputs_.find(input_source);
  if (it != inputs_.end()) {
    for (auto& redirector : audio_output_redirectors_) {
      redirector.second->RemoveInput(it->second.get());
    }
    input = std::move(it->second);
    inputs_.erase(it);
  } else {
    it = ignored_inputs_.find(input_source);
    if (it != ignored_inputs_.end()) {
      input = std::move(it->second);
      ignored_inputs_.erase(it);
    }
  }

  if (input) {
    input->Destroy();
    io_task_runner_->DeleteSoon(FROM_HERE, std::move(input));
  }
  UpdateStreamCountsOnThread();

  if (inputs_.empty()) {
    SetCloseTimeout();
  }
}

void StreamMixer::SetCloseTimeout() {
  close_timestamp_ = (no_input_close_timeout_.is_max()
                          ? base::TimeTicks::Max()
                          : base::TimeTicks::Now() + no_input_close_timeout_);
}

void StreamMixer::UpdateStreamCounts() {
  RUN_ON_MIXER_THREAD(UpdateStreamCountsOnThread);
}

void StreamMixer::UpdateStreamCountsOnThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mixer_sequence_checker_);
  int primary = 0;
  int sfx = 0;
  for (const auto& it : inputs_) {
    MixerInput* input = it.second.get();
    if (input->source()->active() &&
        (input->TargetVolume() > 0.0f || input->InstantaneousVolume() > 0.0f)) {
      (input->primary() ? primary : sfx) += 1;
    }
  }

  if (primary != last_sent_primary_stream_count_ ||
      sfx != last_sent_sfx_stream_count_) {
    last_sent_primary_stream_count_ = primary;
    last_sent_sfx_stream_count_ = sfx;
    receiver_.AsyncCall(&MixerServiceReceiver::OnStreamCountChanged)
        .WithArgs(primary, sfx);
  }
}

MediaPipelineBackend::AudioDecoder::RenderingDelay
StreamMixer::GetTotalRenderingDelay(FilterGroup* filter_group) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mixer_sequence_checker_);
  if (!output_) {
    return MediaPipelineBackend::AudioDecoder::RenderingDelay();
  }
  if (!filter_group) {
    return output_->GetRenderingDelay();
  }

  // Includes |output_->GetRenderingDelay()|.
  return filter_group->GetRenderingDelayToOutput();
}

void StreamMixer::PlaybackLoop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mixer_sequence_checker_);
  if (inputs_.empty() && base::TimeTicks::Now() >= close_timestamp_ &&
      !mixer_pipeline_->IsRinging()) {
    AUDIO_LOG(INFO) << "Close timeout";
    base::AutoLock lock(input_creation_lock_);
    Stop(LoopbackInterruptReason::kOutputStopped);
    return;
  }

  WriteOneBuffer();
  UpdateStreamCountsOnThread();

  mixer_task_runner_->PostTask(FROM_HERE, playback_loop_task_);
}

void StreamMixer::WriteOneBuffer() {
  for (auto& redirector : audio_output_redirectors_) {
    redirector.second->PrepareNextBuffer(redirector_frames_per_write_);
  }

  // Recursively mix and filter each group.
  MediaPipelineBackend::AudioDecoder::RenderingDelay rendering_delay =
      output_->GetRenderingDelay();
  mixer_pipeline_->MixAndFilter(frames_per_write_, rendering_delay);

  int64_t expected_playback_time;
  if (rendering_delay.timestamp_microseconds == kNoTimestamp) {
    expected_playback_time = kNoTimestamp;
  } else {
    expected_playback_time =
        rendering_delay.timestamp_microseconds +
        rendering_delay.delay_microseconds +
        mixer_pipeline_->GetPostLoopbackRenderingDelayMicroseconds();
  }

  for (auto& redirector : audio_output_redirectors_) {
    redirector.second->FinishBuffer();
  }

  WriteMixedPcm(frames_per_write_, expected_playback_time);
}

void StreamMixer::WriteMixedPcm(int frames, int64_t expected_playback_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mixer_sequence_checker_);

  int loopback_channel_count = loopback_channel_mixer_->output_channel_count();
  float* loopback_data = loopback_channel_mixer_->Transform(
      mixer_pipeline_->GetLoopbackOutput(), frames);

  // Hard limit to [1.0, -1.0]
  for (int i = 0; i < frames * loopback_channel_count; ++i) {
    // TODO(bshaya): Warn about clipping here.
    loopback_data[i] = std::clamp(loopback_data[i], -1.0f, 1.0f);
  }

  loopback_handler_->SendData(expected_playback_time,
                              output_samples_per_second_,
                              loopback_channel_count, loopback_data, frames);

  float* linearized_data =
      output_channel_mixer_->Transform(mixer_pipeline_->GetOutput(), frames);

  // Hard limit to [1.0, -1.0].
  for (int i = 0; i < frames * num_output_channels_; ++i) {
    linearized_data[i] = std::clamp(linearized_data[i], -1.0f, 1.0f);
  }

  bool playback_interrupted = false;
  output_->Write(linearized_data, frames * num_output_channels_,
                 &playback_interrupted);

  if (playback_interrupted) {
    loopback_handler_->SendInterrupt(LoopbackInterruptReason::kUnderrun);

    for (const auto& input : inputs_) {
      input.first->OnOutputUnderrun();
    }
  }
}

void StreamMixer::AddAudioOutputRedirector(
    std::unique_ptr<AudioOutputRedirector> redirector) {
  RUN_ON_MIXER_THREAD(AddAudioOutputRedirectorOnThread, std::move(redirector));
}

void StreamMixer::AddAudioOutputRedirectorOnThread(
    std::unique_ptr<AudioOutputRedirector> redirector) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mixer_sequence_checker_);
  AUDIO_LOG(INFO) << __func__;
  DCHECK(redirector);

  AudioOutputRedirector* key = redirector.get();
  audio_output_redirectors_[key] = std::move(redirector);

  if (state_ == kStateRunning) {
    key->SetSampleRate(redirector_samples_per_second_);
  }

  for (const auto& input : inputs_) {
    key->AddInput(input.second.get());
  }
}

void StreamMixer::RemoveAudioOutputRedirector(
    AudioOutputRedirector* redirector) {
  // Always post a task to avoid synchronous deletion.
  RUN_ON_MIXER_THREAD(RemoveAudioOutputRedirectorOnThread, redirector);
}

void StreamMixer::RemoveAudioOutputRedirectorOnThread(
    AudioOutputRedirector* redirector) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mixer_sequence_checker_);
  AUDIO_LOG(INFO) << __func__;
  audio_output_redirectors_.erase(redirector);
}

void StreamMixer::SetVolume(AudioContentType type, float level) {
  RUN_ON_MIXER_THREAD(SetVolumeOnThread, type, level);
}

void StreamMixer::SetVolumeOnThread(AudioContentType type, float level) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mixer_sequence_checker_);
  DCHECK(type != AudioContentType::kOther);

  volume_info_[type].volume = level;
  for (const auto& input : inputs_) {
    if (input.second->content_type() == type) {
      input.second->SetContentTypeVolume(level);
    }
  }
  if (external_audio_pipeline_supported_ && type == AudioContentType::kMedia) {
    ExternalAudioPipelineShlib::SetExternalMediaVolume(
        std::min(level, volume_info_[type].limit));
  }
  UpdateStreamCountsOnThread();
}

void StreamMixer::SetMuted(AudioContentType type, bool muted) {
  RUN_ON_MIXER_THREAD(SetMutedOnThread, type, muted);
}

void StreamMixer::SetMutedOnThread(AudioContentType type, bool muted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mixer_sequence_checker_);
  DCHECK(type != AudioContentType::kOther);

  volume_info_[type].muted = muted;
  for (const auto& input : inputs_) {
    if (input.second->content_type() == type) {
      input.second->SetMuted(muted);
    }
  }
  if (external_audio_pipeline_supported_ && type == AudioContentType::kMedia) {
    ExternalAudioPipelineShlib::SetExternalMediaMuted(muted);
  }
  UpdateStreamCountsOnThread();
}

void StreamMixer::SetOutputLimit(AudioContentType type, float limit) {
  RUN_ON_MIXER_THREAD(SetOutputLimitOnThread, type, limit);
}

void StreamMixer::SetOutputLimitOnThread(AudioContentType type, float limit) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mixer_sequence_checker_);
  DCHECK(type != AudioContentType::kOther);

  AUDIO_LOG(INFO) << "Set volume limit for " << type << " to " << limit;
  volume_info_[type].limit = limit;
  int fade_ms = kUseDefaultFade;
  if (type == AudioContentType::kMedia) {
    if (limit >= 1.0f) {  // Unducking.
      fade_ms = kMediaUnduckFadeMs;
    } else {
      fade_ms = kMediaDuckFadeMs;
    }
  }
  for (const auto& input : inputs_) {
    // Volume limits don't apply to effects streams.
    if (input.second->primary() && input.second->focus_type() == type) {
      input.second->SetOutputLimit(limit, fade_ms);
    }
  }
  if (external_audio_pipeline_supported_ && type == AudioContentType::kMedia) {
    ExternalAudioPipelineShlib::SetExternalMediaVolume(
        std::min(volume_info_[type].volume, limit));
  }
  UpdateStreamCountsOnThread();
}

void StreamMixer::SetVolumeMultiplier(MixerInput::Source* source,
                                      float multiplier) {
  RUN_ON_MIXER_THREAD(SetVolumeMultiplierOnThread, source, multiplier);
}

void StreamMixer::SetVolumeMultiplierOnThread(MixerInput::Source* source,
                                              float multiplier) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mixer_sequence_checker_);
  auto it = inputs_.find(source);
  if (it != inputs_.end()) {
    it->second->SetVolumeMultiplier(multiplier);
  }
  UpdateStreamCountsOnThread();
}

void StreamMixer::SetSimulatedClockRate(MixerInput::Source* source,
                                        double new_clock_rate) {
  RUN_ON_MIXER_THREAD(SetSimulatedClockRateOnThread, source, new_clock_rate);
}

void StreamMixer::SetSimulatedClockRateOnThread(MixerInput::Source* source,
                                                double new_clock_rate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mixer_sequence_checker_);
  auto it = inputs_.find(source);
  if (it != inputs_.end()) {
    it->second->SetSimulatedClockRate(new_clock_rate);
  }
}

void StreamMixer::SetPostProcessorConfig(std::string name, std::string config) {
  RUN_ON_MIXER_THREAD(SetPostProcessorConfigOnThread, std::move(name),
                      std::move(config));
}

void StreamMixer::SetPostProcessorConfigOnThread(std::string name,
                                                 std::string config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mixer_sequence_checker_);
  mixer_pipeline_->SetPostProcessorConfig(name, config);
}

int StreamMixer::GetSampleRateForDeviceId(const std::string& device) {
  DCHECK(mixer_pipeline_);
  return mixer_pipeline_->GetInputGroup(device)->GetInputSampleRate();
}

}  // namespace media
}  // namespace chromecast
