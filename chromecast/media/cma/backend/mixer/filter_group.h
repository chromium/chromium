// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MIXER_FILTER_GROUP_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MIXER_FILTER_GROUP_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chromecast/media/base/aligned_buffer.h"
#include "chromecast/public/media/audio_post_processor2_shlib.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/volume_control.h"

namespace chromecast {
namespace media {
class InterleavedChannelMixer;
class MixerInput;
class PostProcessingPipeline;
class PostProcessingPipelineFactory;

// FilterGroup mixes MixerInputs and/or FilterGroups, mixes their outputs, and
// applies DSP to them.

// Tag to avoid ABA problem.
class FilterGroupTag : public base::RefCountedThreadSafe<FilterGroupTag> {
 private:
  friend class base::RefCountedThreadSafe<FilterGroupTag>;
  ~FilterGroupTag() = default;
};

// FilterGroups are added at construction. These cannot be removed.
class FilterGroup {
 public:
  // |num_channels| indicates number of input audio channels.
  // |name| is used for debug printing
  FilterGroup(int num_channels,
              std::string name,
              base::Value prerender_filter_list,
              const base::Value* filter_list,
              PostProcessingPipelineFactory* ppp_factory,
              const base::Value* volume_limits);

  FilterGroup(const FilterGroup&) = delete;
  FilterGroup& operator=(const FilterGroup&) = delete;

  ~FilterGroup();

  int num_channels() const { return num_channels_; }
  float last_volume() const { return last_volume_; }
  float target_volume() const { return target_volume_; }
  std::string name() const { return name_; }
  AudioContentType content_type() const { return content_type_; }
  int input_frames_per_write() const { return input_frames_per_write_; }
  int input_samples_per_second() const { return input_samples_per_second_; }
  int system_output_sample_rate() const {
    return output_config_.system_output_sample_rate;
  }
  scoped_refptr<FilterGroupTag> tag() const { return tag_; }

  // |input| will be recursively mixed into this FilterGroup's input buffer when
  // MixAndFilter() is called. Registering a FilterGroup as an input to more
  // than one FilterGroup will result in incorrect behavior.
  void AddMixedInput(FilterGroup* input);

  // Recursively sets the sample rate of the post-processors and FilterGroups.
  // This should only be called externally on the output node of the FilterGroup
  // tree.
  // Groups that feed this group may receive different values due to resampling.
  // After calling Initialize(), input_samples_per_second() and
  // input_frames_per_write() may be called to determine the input rate/size.
  void Initialize(const AudioPostProcessor2::Config& output_config);

  // Adds/removes |input| from |active_inputs_|.
  void AddInput(MixerInput* input);
  void RemoveInput(MixerInput* input);

  // Mixes all active inputs and passes them through the audio filter.
  // Returns the largest volume of all streams with data.
  //         return value will be zero IFF there is no data and
  //         the PostProcessingPipeline is not ringing.
  float MixAndFilter(
      int num_frames,
      MediaPipelineBackend::AudioDecoder::RenderingDelay rendering_delay);

  // Gets the current delay of this filter group's AudioPostProcessors.
  // (Not recursive).
  int64_t GetRenderingDelayMicroseconds();

  // Gets the delay of this FilterGroup and all downstream FilterGroups.
  // Computed recursively when MixAndFilter() is called.
  MediaPipelineBackend::AudioDecoder::RenderingDelay
  GetRenderingDelayToOutput();

  std::unique_ptr<PostProcessingPipeline> CreatePrerenderPipeline(
      int num_channels);

  // Retrieves a pointer to the output buffer. This will crash if called before
  // MixAndFilter(), and the data & memory location may change each time
  // MixAndFilter() is called.
  float* GetOutputBuffer();

  // Returns number of audio output channels from the filter group.
  int GetOutputChannelCount() const;

  // Returns the expected sample rate for inputs to this group.
  int GetInputSampleRate() const { return input_samples_per_second_; }

  // Sends configuration string |config| to all post processors with the given
  // |name|.
  void SetPostProcessorConfig(const std::string& name,
                              const std::string& config);

  // Determines whether this group is still ringing out after all input streams
  // have stopped playing.
  bool IsRinging() const;

  // Recursively print the layout of the pipeline.
  void PrintTopology() const;

  // Add |stream_type| to the list of streams this processor handles.
  void AddStreamType(const std::string& stream_type);

 private:
  using VolumeLimitsMap = base::flat_map<std::string, std::pair<float, float>>;

  struct GroupInput {
    GroupInput(FilterGroup* group,
               std::unique_ptr<InterleavedChannelMixer> channel_mixer);
    GroupInput(GroupInput&& other);
    ~GroupInput();

    FilterGroup* group;
    std::unique_ptr<InterleavedChannelMixer> channel_mixer;
  };

  void ParseVolumeLimits(const base::Value* volume_limits);
  void ZeroOutputBufferIfNeeded();
  void ResizeBuffers();

  const int num_channels_;
  const std::string name_;
  base::Value prerender_filter_list_;
  base::flat_map<std::string, std::string> post_processing_configs_;
  PostProcessingPipelineFactory* const ppp_factory_;
  int prerender_creation_count_ = 0;
  const scoped_refptr<FilterGroupTag> tag_;

  VolumeLimitsMap volume_limits_;
  float default_volume_min_ = 0.0f;
  float default_volume_max_ = 1.0f;

  std::vector<GroupInput> mixed_inputs_;
  std::vector<std::string> stream_types_;
  base::flat_set<MixerInput*> active_inputs_;

  AudioPostProcessor2::Config output_config_;
  int input_samples_per_second_ = 0;
  int input_frames_per_write_ = 0;
  int output_frames_zeroed_ = 0;
  float last_volume_ = 0.0f;
  float target_volume_ = 0.0f;
  double delay_seconds_ = 0;
  MediaPipelineBackend::AudioDecoder::RenderingDelay rendering_delay_to_output_;
  AudioContentType content_type_ = AudioContentType::kMedia;

  // Interleaved data must be aligned to 16 bytes.
  AlignedBuffer<float> interleaved_;

  std::unique_ptr<PostProcessingPipeline> post_processing_pipeline_;
  float* output_buffer_ = nullptr;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MIXER_FILTER_GROUP_H_
