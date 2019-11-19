// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MIXER_MIXER_PIPELINE_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MIXER_MIXER_PIPELINE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "chromecast/public/media/audio_post_processor_shlib.h"
#include "chromecast/public/media/media_pipeline_backend.h"

namespace base {
class Value;
}  // namespace base

namespace chromecast {
namespace media {

class FilterGroup;
class PostProcessingPipelineParser;
class PostProcessingPipelineFactory;

// Provides mixer and post-processing functionality for StreamMixer.
// Internally, MixerPipeline is a tree of post processors with two taps -
// LoopbackOutput and Output. Calling MixAndFilter causes the pipeline to pull
// data from inputs, mixing and filtering as described in cast_audio.json.
class MixerPipeline {
 public:
  // Attempts to create a pipeline from |config|.
  // Returns nullptr if config fails to parse.
  static std::unique_ptr<MixerPipeline> CreateMixerPipeline(
      PostProcessingPipelineParser* parser,
      PostProcessingPipelineFactory* factory,
      int expected_input_channels);

  ~MixerPipeline();

  // Sets the sample rate of all processors.
  void Initialize(int samples_per_second, int frames_per_write);

  // Returns the FilterGroup that should process a stream with |device_id| or
  // |nullptr| if no matching FilterGroup is found.
  FilterGroup* GetInputGroup(const std::string& device_id);

  // Polls |MixerInput|s for |frames_per_write| frames of audio data, mixes the
  // inputs, and applies PostProcessors.
  // |rendering_delay| is the rendering delay of the output device, and is used
  // to calculate the delay from various points in the pipeline.
  void MixAndFilter(
      int frames_per_write,
      MediaPipelineBackend::AudioDecoder::RenderingDelay rendering_delay);

  // Returns the output data from the "mix" group.
  float* GetLoopbackOutput();

  // Returns the output data from the "linearize" group.
  float* GetOutput();

  // Returns the number of channels that will be present in GetLoopbackOutput().
  int GetLoopbackChannelCount() const;

  // Returns the number of channels that will be present in GetOutput().
  int GetOutputChannelCount() const;

  // Attempts to send |config| to PostProcessors with |name|.
  void SetPostProcessorConfig(const std::string& name,
                              const std::string& config);

  // Returns the rendering delay between audio coming from GetLoopbackOutput()
  // and GetOutput(), i.e. the group delay of PostProcessors in "linearize"
  int64_t GetPostLoopbackRenderingDelayMicroseconds() const;

  // Informs FilterGroups and PostProcessors which channel will be played out.
  // |playout_channel| may be |-1| to signal all channels will be played out.
  void SetPlayoutChannel(int playout_channel);

  // Determines whether the pipeline is still ringing out after all input
  // streams have stopped playing.
  bool IsRinging() const;

 private:
  // External classes should call CreateMixerPipeline.
  MixerPipeline();

  // Attempts to build a pipeline using |config|. Returns |true| IFF successful.
  bool BuildPipeline(PostProcessingPipelineParser* config,
                     PostProcessingPipelineFactory* factory,
                     int expected_input_channels);

  // Adds |ids| to the list of DeviceIds |filter_group| can process.
  bool SetGroupDeviceIds(const base::Value* ids, FilterGroup* filter_group);

  std::vector<std::unique_ptr<FilterGroup>> filter_groups_;
  base::flat_map<std::string, FilterGroup*> stream_sinks_;
  FilterGroup* loopback_output_group_ = nullptr;
  FilterGroup* output_group_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MixerPipeline);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MIXER_MIXER_PIPELINE_H_
