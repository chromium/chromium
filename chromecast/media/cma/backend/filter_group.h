// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_FILTER_GROUP_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_FILTER_GROUP_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/aligned_memory.h"
#include "base/values.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/volume_control.h"

namespace media {
class AudioBus;
}  // namespace media

namespace chromecast {
namespace media {
class MixerInput;
class PostProcessingPipeline;

// FilterGroup mixes MixerInputs and/or FilterGroups, mixes their outputs, and
// applies DSP to them.

// FilterGroups are added at construction. These cannot be removed.

// InputQueues are added with AddActiveInput(), then cleared when
// MixAndFilter() is called (they must be added each time data is queried).
class FilterGroup {
 public:
  enum class GroupType { kStream, kFinalMix, kLinearize };
  // |num_channels| indicates number of input audio channels.
  // |type| indicates where in the pipeline this FilterGroup sits.
  //    some features are specific to certain locations:
  //     - mono mixer takes place at the end of kFinalMix.
  //     - channel selection occurs before post-processing in kLinearize.
  // |name| is used for debug printing
  // |pipeline| - processing pipeline.
  // |device_ids| is a set of strings that is used as a filter to determine
  //   if an InputQueue belongs to this group (InputQueue->name() must exactly
  //   match an entry in |device_ids| to be processed by this group).
  // |mixed_inputs| are FilterGroups that will be mixed into this FilterGroup.
  //   ex: the final mix ("mix") FilterGroup mixes all other filter groups.
  // FilterGroups currently use either InputQueues OR FilterGroups as inputs,
  //   but there is no technical limitation preventing mixing input classes.

  FilterGroup(int num_channels,
              GroupType type,
              const std::string& name,
              std::unique_ptr<PostProcessingPipeline> pipeline,
              const base::flat_set<std::string>& device_ids,
              const std::vector<FilterGroup*>& mixed_inputs);

  ~FilterGroup();

  // Sets the sample rate of the post-processors.
  void Initialize(int output_samples_per_second);

  // Returns |true| if this FilterGroup is appropriate to process an input with
  // the given |input_device_id|.
  bool CanProcessInput(const std::string& input_device_id);

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

  // Retrieves a pointer to the output buffer. This will crash if called before
  // MixAndFilter(), and the data & memory location may change each time
  // MixAndFilter() is called.
  float* GetOutputBuffer();

  // Get the last used volume.
  float last_volume() const { return last_volume_; }

  std::string name() const { return name_; }

  // Returns number of audio output channels from the filter group.
  int GetOutputChannelCount();

  // Sends configuration string |config| to all post processors with the given
  // |name|.
  void SetPostProcessorConfig(const std::string& name,
                              const std::string& config);

  // Toggles the mono mixer.
  void SetMixToMono(bool mix_to_mono);

  // Sets the active channel.
  void UpdatePlayoutChannel(int playout_channel);

  // Get content type
  AudioContentType content_type() const { return content_type_; }

 private:
  // Resizes temp_ and mixed_ if they are too small to hold |num_frames| frames.
  // Returns |true| if |num_frames| is larger than all previous |num_frames|.
  bool ResizeBuffersIfNecessary(int num_frames);
  void AddTempBuffer(int num_channels, int num_frames);

  const int num_channels_;
  const GroupType type_;
  bool mix_to_mono_;
  int playout_channel_;
  const std::string name_;
  const base::flat_set<std::string> device_ids_;
  std::vector<FilterGroup*> mixed_inputs_;
  base::flat_set<MixerInput*> active_inputs_;

  int output_samples_per_second_;
  int frames_zeroed_;
  float last_volume_;
  int64_t delay_frames_;
  MediaPipelineBackend::AudioDecoder::RenderingDelay rendering_delay_to_output_;
  AudioContentType content_type_;

  // Buffers that hold audio data while it is mixed.
  // These are kept as members of this class to minimize copies and
  // allocations.
  std::vector<std::unique_ptr<::media::AudioBus>> temp_buffers_;
  std::unique_ptr<::media::AudioBus> mixed_;

  // Interleaved data must be aligned to 16 bytes.
  std::unique_ptr<float, base::AlignedFreeDeleter> interleaved_;

  std::unique_ptr<PostProcessingPipeline> post_processing_pipeline_;

  DISALLOW_COPY_AND_ASSIGN(FilterGroup);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_FILTER_GROUP_H_
