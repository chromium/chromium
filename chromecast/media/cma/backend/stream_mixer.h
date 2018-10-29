// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_STREAM_MIXER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_STREAM_MIXER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "chromecast/media/cma/backend/mixer_input.h"
#include "chromecast/media/cma/backend/mixer_pipeline.h"
#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/public/media/external_audio_pipeline_shlib.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/volume_control.h"

namespace chromecast {
namespace media {

class AudioOutputRedirector;
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
//
// We use a "shim thread" to avoid priority inversion due to PostTask to
// low-priority threads. Suppose we want to PostTask to some low-priority
// thread, and the internal PostTask mutex is locked by that thread (and the
// low-priority thread isn't running right now). If the mixer thread does the
// PostTask, then it will block until the low-priority thread gets to run again
// and unlocks the mutex. Instead, we can PostTask to the shim thread (which is
// high-priority, and won't be holding the mutex for significant periods) and so
// avoid blocking waiting for the low-priority thread. The shim thread will
// maybe block when it does the PostTask to the low-priority thread, but we
// don't really care about that.
// All loopback audio handling is done on the shim thread, and any tasks posted
// from/to potentially low-priority threads should be done through the shim
// thread.
class StreamMixer {
 public:
  // Returns the mixer instance for this process. Caller must not delete the
  // returned instance!
  static StreamMixer* Get();

  StreamMixer();
  // Only public to allow tests to create/destroy mixers.
  ~StreamMixer();

  // Adds an input source to the mixer. The |input_source| must live at least
  // until input_source->FinalizeAudioPlayback() is called.
  void AddInput(MixerInput::Source* input_source);
  // Removes an input source from the mixer. The input source will be removed
  // asynchronously. Once it has been removed, FinalizeAudioPlayback() will be
  // called on it; at this point it is safe to delete the input source.
  void RemoveInput(MixerInput::Source* input_source);

  // Adds/removes a loopback audio observer. Observers are passed audio data
  // just before it is written to output (prior to linearization filters).
  void AddLoopbackAudioObserver(
      CastMediaShlib::LoopbackAudioObserver* observer);
  void RemoveLoopbackAudioObserver(
      CastMediaShlib::LoopbackAudioObserver* observer);

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

  // Sends configuration string |config| to processor |name|.
  void SetPostProcessorConfig(const std::string& name,
                              const std::string& config);

  void ResetPostProcessors(CastMediaShlib::ResultCallback callback);

  // Test-only methods.
  StreamMixer(std::unique_ptr<MixerOutputStream> output,
              std::unique_ptr<base::Thread> mixer_thread,
              scoped_refptr<base::SingleThreadTaskRunner> mixer_task_runner);
  void ResetPostProcessorsForTest(
      std::unique_ptr<PostProcessingPipelineFactory> pipeline_factory,
      const std::string& pipeline_json);
  void SetNumOutputChannelsForTest(int num_output_channels);
  void ValidatePostProcessorsForTest();
  int num_output_channels() const { return num_output_channels_; }

  scoped_refptr<base::SingleThreadTaskRunner> shim_task_runner() const {
    return shim_task_runner_;
  }

 private:
  class ExternalLoopbackAudioObserver;
  class BaseExternalMediaVolumeChangeRequestObserver
      : public ExternalAudioPipelineShlib::
            ExternalMediaVolumeChangeRequestObserver {
   public:
    ~BaseExternalMediaVolumeChangeRequestObserver() override = default;
  };
  class ExternalMediaVolumeChangeRequestObserver;

  enum State {
    kStateStopped,
    kStateRunning,
  };

  // Contains volume control information for an audio content type.
  struct VolumeInfo {
    float GetEffectiveVolume();

    float volume = 0.0f;
    float limit = 1.0f;
    bool muted = false;
  };

  void ResetPostProcessorsOnThread(CastMediaShlib::ResultCallback callback,
                                   const std::string& override_config);
  void CreatePostProcessors(CastMediaShlib::ResultCallback callback,
                            const std::string& override_config);
  bool PostProcessorsHaveCorrectNumOutputs();
  void FinalizeOnMixerThread();
  void Start();
  void Stop();
  void CheckChangeOutputRate(int input_samples_per_second);
  void SignalError(MixerInput::Source::MixerError error);
  void AddInputOnThread(MixerInput::Source* input_source);
  void RemoveInputOnThread(MixerInput::Source* input_source);
  void SetCloseTimeout();
  void UpdatePlayoutChannel();

  void PlaybackLoop();
  void WriteOneBuffer();
  void WriteMixedPcm(int frames, int64_t expected_playback_time);

  void SetVolumeOnThread(AudioContentType type, float level);
  void SetMutedOnThread(AudioContentType type, bool muted);
  void SetOutputLimitOnThread(AudioContentType type, float limit);
  void SetVolumeMultiplierOnThread(MixerInput::Source* source,
                                   float multiplier);
  void SetPostProcessorConfigOnThread(const std::string& name,
                                      const std::string& config);

  void AddLoopbackAudioObserverOnShimThread(
      CastMediaShlib::LoopbackAudioObserver* observer);
  void RemoveLoopbackAudioObserverOnShimThread(
      CastMediaShlib::LoopbackAudioObserver* observer);

  void AddAudioOutputRedirectorOnThread(
      std::unique_ptr<AudioOutputRedirector> redirector);
  void RemoveAudioOutputRedirectorOnThread(AudioOutputRedirector* redirector);

  void PostLoopbackData(int64_t expected_playback_time,
                        SampleFormat sample_format,
                        int sample_rate,
                        int channels,
                        std::unique_ptr<uint8_t[]> data,
                        int length);
  void PostLoopbackInterrupted();

  void SendLoopbackData(int64_t expected_playback_time,
                        SampleFormat sample_format,
                        int sample_rate,
                        int channels,
                        std::unique_ptr<uint8_t[]> data,
                        int length);
  void LoopbackInterrupted();

  MediaPipelineBackend::AudioDecoder::RenderingDelay GetTotalRenderingDelay(
      FilterGroup* filter_group);

  std::unique_ptr<MixerOutputStream> output_;
  std::unique_ptr<PostProcessingPipelineFactory>
      post_processing_pipeline_factory_;
  std::unique_ptr<MixerPipeline> mixer_pipeline_;
  std::unique_ptr<base::Thread> mixer_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> mixer_task_runner_;

  // Special thread to avoid underruns due to priority inversion.
  std::unique_ptr<base::Thread> shim_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> shim_task_runner_;
  std::unique_ptr<base::Thread> input_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;

  int num_output_channels_;
  const int low_sample_rate_cutoff_;
  const int fixed_sample_rate_;
  const base::TimeDelta no_input_close_timeout_;
  // Force data to be filtered in multiples of |filter_frame_alignment_| frames.
  // Must be a multiple of 4 for some NEON implementations. Some
  // AudioPostProcessors require stricter alignment conditions.
  const int filter_frame_alignment_;

  int requested_output_samples_per_second_ = 0;
  int output_samples_per_second_ = 0;
  int frames_per_write_ = 0;

  State state_;
  base::TimeTicks close_timestamp_;

  base::flat_map<MixerInput::Source*, std::unique_ptr<MixerInput>> inputs_;
  base::flat_map<MixerInput::Source*, std::unique_ptr<MixerInput>>
      ignored_inputs_;

  base::flat_set<CastMediaShlib::LoopbackAudioObserver*> loopback_observers_;

  base::flat_map<AudioContentType, VolumeInfo> volume_info_;

  base::flat_map<AudioOutputRedirector*, std::unique_ptr<AudioOutputRedirector>>
      audio_output_redirectors_;

  const bool external_audio_pipeline_supported_;
  std::unique_ptr<BaseExternalMediaVolumeChangeRequestObserver>
      external_volume_observer_;
  std::unique_ptr<ExternalLoopbackAudioObserver>
      external_loopback_audio_observer_;

  base::WeakPtrFactory<StreamMixer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(StreamMixer);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_STREAM_MIXER_H_
