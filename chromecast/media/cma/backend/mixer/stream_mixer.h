// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MIXER_STREAM_MIXER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MIXER_STREAM_MIXER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "chromecast/media/cma/backend/mixer/mixer_input.h"
#include "chromecast/media/cma/backend/mixer/mixer_pipeline.h"
#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/public/media/external_audio_pipeline_shlib.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/volume_control.h"

namespace chromecast {
class ThreadHealthChecker;

namespace media {

class AudioOutputRedirector;
class InterleavedChannelMixer;
class LoopbackHandler;
enum class LoopbackInterruptReason;
class MixerServiceReceiver;
class MixerOutputStream;
class PostProcessingPipelineFactory;

// Mixer implementation. The mixer has zero or more inputs; these can be added
// or removed at any time. The mixer will pull from each input source whenever
// it needs more data; input sources provide data if available, and return the
// number of frames filled. Any unfilled frames will be filled with silence;
// the mixer always outputs audio when active, even if input sources underflow.
// The MixerInput class wraps each source and provides volume control and
// resampling to the output rate. The mixer will pass the current rendering
// delay to each source when it requests data; this delay indicates when the new
// data will play out of the speaker (the newly filled data will play out at
// delay.timestamp + delay.delay).
//
// The mixer chooses its output sample rate based on the sample rates of the
// current sources (unless the output sample rate is fixed via the
// --audio-output-sample-rate command line flag). When a new source is added:
//  * If the mixer was not running, it is started with the output sample rate
//    set to the input sample rate of the new source.
//  * If the mixer previously had no input sources, the output sample rate is
//    updated to match the input sample rate of the new source.
//  * If the new input source is primary, and the mixer has no other primary
//    input sources, then the output sample rate is updated to match the input
//    sample rate of the new source.
//  * Otherwise, the output sample rate remains unchanged.
class StreamMixer {
 public:
  StreamMixer(
      scoped_refptr<base::SequencedTaskRunner> io_task_runner = nullptr);

  StreamMixer(const StreamMixer&) = delete;
  StreamMixer& operator=(const StreamMixer&) = delete;

  ~StreamMixer();

  int num_output_channels() const { return num_output_channels_; }

  scoped_refptr<base::TaskRunner> task_runner() const {
    return mixer_task_runner_;
  }

  // Adds an input source to the mixer. The |input_source| must live at least
  // until input_source->FinalizeAudioPlayback() is called.
  void AddInput(MixerInput::Source* input_source)
      LOCKS_EXCLUDED(input_creation_lock_);
  // Removes an input source from the mixer. The input source will be removed
  // asynchronously. Once it has been removed, FinalizeAudioPlayback() will be
  // called on it; at this point it is safe to delete the input source.
  void RemoveInput(MixerInput::Source* input_source);

  // Adds/removes an output redirector. Output redirectors take audio from
  // matching inputs and pass them to secondary output instead of the normal
  // mixer output.
  void AddAudioOutputRedirector(
      std::unique_ptr<AudioOutputRedirector> redirector);
  void RemoveAudioOutputRedirector(AudioOutputRedirector* redirector);

  // Sets the volume multiplier for the given content |type|.
  void SetVolume(AudioContentType type, float level);

  // Sets the mute state for the given content |type|.
  void SetMuted(AudioContentType type, bool muted);

  // Sets the volume multiplier limit for the given content |type|.
  void SetOutputLimit(AudioContentType type, float limit);

  // Sets the per-stream volume multiplier for |source|.
  void SetVolumeMultiplier(MixerInput::Source* source, float multiplier);

  // Sets the simulated audio clock rate for |source|.
  void SetSimulatedClockRate(MixerInput::Source* source, double new_clock_rate);

  // Sends configuration string |config| to processor |name|.
  void SetPostProcessorConfig(std::string name, std::string config);

  void ResetPostProcessors(CastMediaShlib::ResultCallback callback);

  // Updates the counts of active streams and signals any observing control
  // connections.
  void UpdateStreamCounts();

  // Sets the desired number of output channels that the mixer should use. The
  // actual number of output channels may differ from this value.
  void SetNumOutputChannels(int num_channels);

  // Test-only methods.
  StreamMixer(
      std::unique_ptr<MixerOutputStream> output,
      scoped_refptr<base::SequencedTaskRunner> mixer_task_runner,
      const std::string& pipeline_json,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner = nullptr);
  void ResetPostProcessorsForTest(
      std::unique_ptr<PostProcessingPipelineFactory> pipeline_factory,
      const std::string& pipeline_json);
  void SetNumOutputChannelsForTest(int num_output_channels);
  void EnableDynamicChannelCountForTest(bool enable);
  LoopbackHandler* GetLoopbackHandlerForTest();

 private:
  class BaseExternalMediaVolumeChangeRequestObserver
      : public ExternalAudioPipelineShlib::
            ExternalMediaVolumeChangeRequestObserver {
   public:
    ~BaseExternalMediaVolumeChangeRequestObserver() override = default;
  };
  class ExternalMediaVolumeChangeRequestObserver;

  class MixerThread;

  enum State {
    kStateStopped,
    kStateRunning,
  };

  // Contains volume control information for an audio content type.
  struct VolumeInfo {
    float volume = 0.0f;
    float limit = 1.0f;
    bool muted = false;
  };

  void SetNumOutputChannelsOnThread(int num_channels)
      LOCKS_EXCLUDED(input_creation_lock_);
  void ResetPostProcessorsOnThread(CastMediaShlib::ResultCallback callback,
                                   const std::string& override_config)
      LOCKS_EXCLUDED(input_creation_lock_);
  void CreatePostProcessors(CastMediaShlib::ResultCallback callback,
                            const std::string& override_config,
                            int expected_input_channels)
      EXCLUSIVE_LOCKS_REQUIRED(input_creation_lock_);
  void FinalizeOnMixerThread() LOCKS_EXCLUDED(input_creation_lock_);
  void Start() LOCKS_EXCLUDED(input_creation_lock_);
  void Stop(LoopbackInterruptReason reason)
      EXCLUSIVE_LOCKS_REQUIRED(input_creation_lock_);
  void CheckChangeOutputParams(int num_input_channels,
                               int input_samples_per_second)
      LOCKS_EXCLUDED(input_creation_lock_);
  void SignalError(MixerInput::Source::MixerError error);
  int GetEffectiveChannelCount(MixerInput::Source* input_source);
  std::unique_ptr<MixerInput> CreateInput(MixerInput::Source* input_source);
  void AddInputOnThread(MixerInput::Source* input_source,
                        std::unique_ptr<MixerInput> input);
  void RemoveInputOnThread(MixerInput::Source* input_source);
  void SetCloseTimeout();

  void PlaybackLoop() LOCKS_EXCLUDED(input_creation_lock_);
  void WriteOneBuffer();
  void WriteMixedPcm(int frames, int64_t expected_playback_time);

  void UpdateStreamCountsOnThread();
  void SetVolumeOnThread(AudioContentType type, float level);
  void SetMutedOnThread(AudioContentType type, bool muted);
  void SetOutputLimitOnThread(AudioContentType type, float limit);
  void SetVolumeMultiplierOnThread(MixerInput::Source* source,
                                   float multiplier);
  void SetSimulatedClockRateOnThread(MixerInput::Source* source,
                                     double new_clock_rate);
  void SetPostProcessorConfigOnThread(std::string name, std::string config);
  void AddAudioOutputRedirectorOnThread(
      std::unique_ptr<AudioOutputRedirector> redirector);
  void RemoveAudioOutputRedirectorOnThread(AudioOutputRedirector* redirector);

  int GetSampleRateForDeviceId(const std::string& device);

  void OnHealthCheckFailed();

  MediaPipelineBackend::AudioDecoder::RenderingDelay GetTotalRenderingDelay(
      FilterGroup* filter_group);

  std::unique_ptr<MixerOutputStream> output_;
  std::unique_ptr<PostProcessingPipelineFactory>
      post_processing_pipeline_factory_;
  std::unique_ptr<MixerPipeline> mixer_pipeline_;
  SEQUENCE_CHECKER(mixer_sequence_checker_);
  scoped_refptr<MixerThread> mixer_thread_;
  scoped_refptr<base::TaskRunner> mixer_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  std::unique_ptr<ThreadHealthChecker> health_checker_;

  std::unique_ptr<InterleavedChannelMixer> loopback_channel_mixer_;
  std::unique_ptr<InterleavedChannelMixer> output_channel_mixer_;

  bool enable_dynamic_channel_count_;
  const int low_sample_rate_cutoff_;
  int fixed_num_output_channels_;
  const int fixed_output_sample_rate_;
  const base::TimeDelta no_input_close_timeout_;
  // Force data to be filtered in multiples of |filter_frame_alignment_| frames.
  // Must be a multiple of 4 for some NEON implementations. Some
  // AudioPostProcessors require stricter alignment conditions.
  const int filter_frame_alignment_;

  int requested_input_channels_ = 0;
  int post_processor_input_channels_ = 0;
  int num_output_channels_ = 0;

  int requested_output_samples_per_second_ = 0;
  int output_samples_per_second_ = 0;
  int frames_per_write_ = 0;

  int redirector_samples_per_second_ = 0;
  int redirector_frames_per_write_ = 0;

  int last_sent_primary_stream_count_ = 0;
  int last_sent_sfx_stream_count_ = 0;

  State state_;
  base::TimeTicks close_timestamp_;

  base::Lock input_creation_lock_;

  base::RepeatingClosure playback_loop_task_;
  base::flat_map<MixerInput::Source*, std::unique_ptr<MixerInput>> inputs_;
  base::flat_map<MixerInput::Source*, std::unique_ptr<MixerInput>>
      ignored_inputs_;

  base::flat_map<AudioContentType, VolumeInfo> volume_info_;

  base::flat_map<AudioOutputRedirector*, std::unique_ptr<AudioOutputRedirector>>
      audio_output_redirectors_;

  const bool external_audio_pipeline_supported_;
  std::unique_ptr<BaseExternalMediaVolumeChangeRequestObserver>
      external_volume_observer_;

  std::unique_ptr<LoopbackHandler> loopback_handler_;
  base::SequenceBound<MixerServiceReceiver> receiver_;

  base::WeakPtrFactory<StreamMixer> weak_factory_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MIXER_STREAM_MIXER_H_
