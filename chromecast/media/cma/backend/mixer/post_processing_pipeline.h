// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MIXER_POST_PROCESSING_PIPELINE_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MIXER_POST_PROCESSING_PIPELINE_H_

#include <memory>
#include <string>
#include <vector>

#include "chromecast/public/media/audio_post_processor2_shlib.h"
#include "chromecast/public/volume_control.h"

namespace base {
class Value;
}  // namespace base

namespace chromecast {
namespace media {

class PostProcessingPipeline {
 public:
  virtual ~PostProcessingPipeline() = default;

  virtual void ProcessFrames(float* data,
                             int num_frames,
                             float current_multiplier,
                             float target_volume,
                             bool is_silence) = 0;
  virtual float* GetOutputBuffer() = 0;
  virtual int NumOutputChannels() const = 0;

  virtual bool SetOutputConfig(
      const AudioPostProcessor2::Config& output_config) = 0;
  virtual int GetInputSampleRate() const = 0;
  virtual bool IsRinging() = 0;
  virtual void SetPostProcessorConfig(const std::string& name,
                                      const std::string& config) = 0;
  virtual void SetContentType(AudioContentType content_type) = 0;
  virtual void UpdatePlayoutChannel(int channel) = 0;
  // Returns the rendering delay in seconds.
  virtual double GetDelaySeconds() = 0;
};

class PostProcessingPipelineFactory {
 public:
  virtual ~PostProcessingPipelineFactory() = default;

  virtual std::unique_ptr<PostProcessingPipeline> CreatePipeline(
      const std::string& name,
      const base::Value* filter_description_list,
      int num_channels) = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MIXER_POST_PROCESSING_PIPELINE_H_
