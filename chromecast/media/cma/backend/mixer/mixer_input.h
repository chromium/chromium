// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MIXER_MIXER_INPUT_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MIXER_MIXER_INPUT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "chromecast/media/base/slew_volume.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/volume_control.h"

namespace media {
class AudioBus;
class ChannelMixer;
class MultiChannelResampler;
}  // namespace media

namespace chromecast {
namespace media {

class AudioOutputRedirectorInput;
class FilterGroup;

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

    // TODO(b/139311908) Track channel layout.
    virtual int num_channels() = 0;
    virtual int input_samples_per_second() = 0;
    virtual bool primary() = 0;
    virtual const std::string& device_id() = 0;
    virtual AudioContentType content_type() = 0;
    virtual int desired_read_size() = 0;
    virtual int playout_channel() = 0;
    // Returns true if the source is currently providing audio to be mixed.
    virtual bool active() = 0;

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

    // Called when the mixer has finished removing this input. The source may be
    // deleted at this point.
    virtual void FinalizeAudioPlayback() = 0;

   protected:
    virtual ~Source() = default;
  };

  MixerInput(Source* source, FilterGroup* filter_group);
  ~MixerInput();

  void SetFilterGroup(FilterGroup* filter_group);

  Source* source() const { return source_; }
  int num_channels() const { return num_channels_; }
  int input_samples_per_second() const { return input_samples_per_second_; }
  int output_samples_per_second() const { return output_samples_per_second_; }
  bool primary() const { return primary_; }
  const std::string& device_id() const { return device_id_; }
  AudioContentType content_type() const { return content_type_; }

  // Adds/removes an output redirector. When the mixer asks for more audio data,
  // the lowest-ordered redirector (based on redirector->GetOrder()) is passed
  // the audio data that would ordinarily have been mixed for local output;
  // no audio from this MixerInput is passed to the mixer.
  void AddAudioOutputRedirector(AudioOutputRedirectorInput* redirector);
  void RemoveAudioOutputRedirector(AudioOutputRedirectorInput* redirector);

  // Reads data from the source. Returns the number of frames actually filled
  // (<= |num_frames|).
  int FillAudioData(int num_frames,
                    RenderingDelay rendering_delay,
                    ::media::AudioBus* dest);

  // Propagates |error| to the source.
  void SignalError(Source::MixerError error);

  // Scales |frames| frames at |src| by the current volume (smoothing as
  // needed). Adds the scaled result to |dest|.
  // VolumeScaleAccumulate will be called once for each channel of audio
  // present.
  // |src| and |dest| should be 16-byte aligned.
  void VolumeScaleAccumulate(const float* src, int frames, float* dest);

  // Sets the per-stream volume multiplier. If |multiplier| < 0, sets the
  // volume multiplier to 0.
  void SetVolumeMultiplier(float multiplier);

  // Sets the multiplier based on this stream's content type. The resulting
  // output volume should be the content type volume * the per-stream volume
  // multiplier. If |fade_ms| is >= 0, the volume change should be faded over
  // that many milliseconds; otherwise, the default fade time should be used.
  void SetContentTypeVolume(float volume, int fade_ms);

  // Sets whether or not this stream should be muted.
  void SetMuted(bool muted);

  // Returns the target volume multiplier of the stream. Fading in or out may
  // cause this to be different from the actual multiplier applied in the last
  // buffer. For the actual multiplier applied, use InstantaneousVolume().
  float TargetVolume();

  // Returns the largest volume multiplier applied to the last buffer
  // retrieved. This differs from TargetVolume() during transients.
  float InstantaneousVolume();

 private:
  int FillBuffer(int num_frames,
                 RenderingDelay rendering_delay,
                 ::media::AudioBus* dest);

  void ResamplerReadCallback(int frame_delay, ::media::AudioBus* output);

  Source* const source_;
  const int num_channels_;
  const int input_samples_per_second_;
  const int output_samples_per_second_;
  const bool primary_;
  const std::string device_id_;
  const AudioContentType content_type_;

  FilterGroup* filter_group_ = nullptr;
  std::unique_ptr<::media::AudioBus> fill_buffer_;
  std::unique_ptr<::media::ChannelMixer> channel_mixer_;

  float stream_volume_multiplier_;
  float type_volume_multiplier_;
  float mute_volume_multiplier_;
  SlewVolume slew_volume_;
  // True if volume scale-accumulate has already been applied for at least
  // one channel of the current buffer.
  bool volume_applied_;
  bool previous_ended_in_silence_;
  bool first_buffer_;

  RenderingDelay mixer_rendering_delay_;
  double resampler_buffered_frames_;
  std::unique_ptr<::media::MultiChannelResampler> resampler_;

  std::vector<AudioOutputRedirectorInput*> audio_output_redirectors_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(MixerInput);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MIXER_MIXER_INPUT_H_
