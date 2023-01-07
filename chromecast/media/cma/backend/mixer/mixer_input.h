// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MIXER_MIXER_INPUT_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MIXER_MIXER_INPUT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chromecast/media/base/aligned_buffer.h"
#include "chromecast/media/base/slew_volume.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/volume_control.h"
#include "media/base/channel_layout.h"

namespace media {
class AudioBus;
class MultiChannelResampler;
}  // namespace media

namespace chromecast {
namespace media {

class AudioOutputRedirectorInput;
class FilterGroup;
class FilterGroupTag;
class InterleavedChannelMixer;
class PostProcessingPipeline;

// Input stream to the mixer. Handles pulling data from the data source and
// resampling it to the mixer's output sample rate, as well as volume control.
// All methods must be called on the mixer thread.
class MixerInput {
 public:
  using RenderingDelay = MediaPipelineBackend::AudioDecoder::RenderingDelay;

  // Data source for the mixer. All methods are called on the mixer thread and
  // must return promptly to avoid audio underruns. The source must remain valid
  // until FinalizeAudioPlayback() is called.
  class Source {
   public:
    enum class MixerError {
      // This input is being ignored due to a sample rate change.
      kInputIgnored,
      // An internal mixer error occurred. The input is no longer usable.
      kInternalError,
    };

    virtual size_t num_channels() const = 0;
    virtual ::media::ChannelLayout channel_layout() const = 0;
    virtual int sample_rate() const = 0;
    virtual bool primary() = 0;
    virtual const std::string& device_id() = 0;
    virtual AudioContentType content_type() = 0;
    virtual AudioContentType focus_type() = 0;
    virtual int desired_read_size() = 0;
    virtual int playout_channel() = 0;
    // Returns true if the source is currently providing audio to be mixed.
    virtual bool active() = 0;
    virtual bool require_clock_rate_simulation() const = 0;

    // Called when the input has been added to the mixer, before any other
    // calls are made. The |read_size| is the number of frames that will be
    // requested for each call to FillAudioPlaybackFrames(). The
    // |initial_rendering_delay| is the rendering delay estimate for the first
    // call to FillAudioPlaybackFrames().
    virtual void InitializeAudioPlayback(
        int read_size,
        RenderingDelay initial_rendering_delay) = 0;

    // Called to read more audio data from the source. The source must fill in
    // |buffer| with up to |num_frames| of audio. The |rendering_delay|
    // indicates when the first frame of the filled data will be played out.
    // Returns the number of frames filled into |buffer|.
    virtual int FillAudioPlaybackFrames(int num_frames,
                                        RenderingDelay rendering_delay,
                                        ::media::AudioBus* buffer) = 0;

    // Called when a mixer error occurs. No more data will be pulled from the
    // source.
    virtual void OnAudioPlaybackError(MixerError error) = 0;

    // Called when an underrun error occurs on mixer output.
    virtual void OnOutputUnderrun() {}

    // Called when the mixer has finished removing this input. The source may be
    // deleted at this point.
    virtual void FinalizeAudioPlayback() = 0;

   protected:
    virtual ~Source() = default;
  };

  MixerInput(Source* source, FilterGroup* filter_group);

  MixerInput(const MixerInput&) = delete;
  MixerInput& operator=(const MixerInput&) = delete;

  ~MixerInput();

  void Initialize();
  void Destroy();
  void SetFilterGroup(FilterGroup* filter_group);

  void SetPostProcessorConfig(const std::string& name,
                              const std::string& config);

  Source* source() const { return source_; }
  int num_channels() const { return num_channels_; }
  ::media::ChannelLayout channel_layout() const { return channel_layout_; }
  int input_samples_per_second() const { return input_samples_per_second_; }
  int output_samples_per_second() const { return output_samples_per_second_; }
  bool primary() const { return primary_; }
  const std::string& device_id() const { return device_id_; }
  AudioContentType content_type() const { return content_type_; }
  AudioContentType focus_type() const { return source_->focus_type(); }
  bool has_render_output() const { return (render_output_ != nullptr); }

  // Adds/removes an output redirector. When the mixer asks for more audio data,
  // the lowest-ordered redirector (based on redirector->GetOrder()) is passed
  // the audio data that would ordinarily have been mixed for local output;
  // no audio from this MixerInput is passed to the mixer.
  void AddAudioOutputRedirector(AudioOutputRedirectorInput* redirector);
  void RemoveAudioOutputRedirector(AudioOutputRedirectorInput* redirector);

  // Renders data from the source. Returns |true| if any audio was rendered.
  bool Render(
      int num_output_frames,
      MediaPipelineBackend::AudioDecoder::RenderingDelay rendering_delay);
  float* RenderedAudioBuffer();

  // Propagates |error| to the source.
  void SignalError(Source::MixerError error);

  // Scales |frames| frames at |src| by the current volume (smoothing as
  // needed). Adds the scaled result to |dest|.
  // VolumeScaleAccumulate will be called once for each channel of audio
  // present.
  // |src| and |dest| should be 16-byte aligned.
  void VolumeScaleAccumulate(const float* src,
                             int frames,
                             float* dest,
                             int channel_index);

  // Sets the per-stream volume multiplier. If |multiplier| < 0, sets the
  // volume multiplier to 0.
  void SetVolumeMultiplier(float multiplier);

  // Sets the multiplier based on this stream's content type. The resulting
  // output volume should be the content type volume * the per-stream volume
  // multiplier.
  void SetContentTypeVolume(float volume);

  // Sets min/max output volume for this stream (ie, limits the product of
  // content type volume and per-stream volume multiplier). Note that mute
  // and runtime output limits (for ducking) are applied after these limits.
  void SetVolumeLimits(float volume_min, float volume_max);

  // Limits the output volume for this stream to below |limit|. Used for
  // ducking. If |fade_ms| is >= 0, the resulting volume change should be
  // faded over that many milliseconds; otherwise, the default fade time should
  // be used.
  void SetOutputLimit(float limit, int fade_ms);

  // Sets whether or not this stream should be muted.
  void SetMuted(bool muted);

  // Returns the target volume multiplier of the stream. Fading in or out may
  // cause this to be different from the actual multiplier applied in the last
  // buffer. For the actual multiplier applied, use InstantaneousVolume().
  float TargetVolume();

  // Returns the largest volume multiplier applied to the last buffer
  // retrieved. This differs from TargetVolume() during transients.
  float InstantaneousVolume();

  // Sets the simulated audio clock rate (by changing the resample rate).
  void SetSimulatedClockRate(double new_clock_rate);

 private:
  bool SetFilterGroupInternal(FilterGroup* filter_group);
  void CreateChannelMixer(int playout_channel, FilterGroup* filter_group);
  // Reads data from the source. Returns the number of frames actually filled
  // (<= |num_frames|).
  int FillAudioData(int num_frames,
                    RenderingDelay rendering_delay,
                    ::media::AudioBus* dest);
  int FillBuffer(int num_frames,
                 RenderingDelay rendering_delay,
                 ::media::AudioBus* dest);
  void RenderInterleaved(int num_output_frames);

  void ResamplerReadCallback(int frame_delay, ::media::AudioBus* output);

  Source* const source_;
  const int num_channels_;
  const ::media::ChannelLayout channel_layout_;
  const int input_samples_per_second_;
  const int output_samples_per_second_;
  const bool primary_;
  const std::string device_id_;
  const AudioContentType content_type_;
  int source_read_size_ = 0;
  int playout_channel_ = 0;

  FilterGroup* filter_group_ = nullptr;
  scoped_refptr<FilterGroupTag> filter_group_tag_;
  std::unique_ptr<::media::AudioBus> fill_buffer_;
  AlignedBuffer<float> interleaved_;
  std::unique_ptr<InterleavedChannelMixer> channel_mixer_;

  float stream_volume_multiplier_ = 1.0f;
  float type_volume_multiplier_ = 1.0f;
  float volume_min_ = 0.0f;
  float volume_max_ = 1.0f;
  float output_volume_limit_ = 1.0f;
  float mute_volume_multiplier_ = 1.0f;
  SlewVolume slew_volume_;
  bool incomplete_previous_fill_ = false;
  bool previous_ended_in_silence_ = false;
  bool first_buffer_ = true;

  RenderingDelay mixer_rendering_delay_;
  double resampler_buffered_frames_ = 0.0;
  int filled_for_resampler_;
  bool tried_to_fill_resampler_;
  int resampled_silence_count_ = 0;
  std::unique_ptr<::media::MultiChannelResampler> resampler_;
  double resample_ratio_ = 1.0;
  double simulated_clock_rate_ = 1.0;

  std::unique_ptr<PostProcessingPipeline> prerender_pipeline_;
  float* render_output_ = nullptr;
  double prerender_delay_seconds_ = 0.0;

  std::vector<AudioOutputRedirectorInput*> audio_output_redirectors_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MIXER_MIXER_INPUT_H_
