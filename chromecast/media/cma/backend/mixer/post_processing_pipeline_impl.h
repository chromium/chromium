// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MIXER_POST_PROCESSING_PIPELINE_IMPL_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MIXER_POST_PROCESSING_PIPELINE_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "chromecast/media/base/aligned_buffer.h"
#include "chromecast/media/cma/backend/mixer/post_processing_pipeline.h"
#include "chromecast/media/cma/backend/mixer/post_processor_factory.h"
#include "chromecast/public/volume_control.h"

namespace base {
class Value;
}  // namespace base

namespace chromecast {
namespace media {

class AudioPostProcessor2;

// Creates and contains multiple AudioPostProcessors, as specified in ctor.
// Provides convenience methods to access and use the AudioPostProcessors.
class PostProcessingPipelineImpl : public PostProcessingPipeline {
 public:
  PostProcessingPipelineImpl(const std::string& name,
                             const base::Value* filter_description_list,
                             int channels);
  ~PostProcessingPipelineImpl() override;

  double ProcessFrames(float* data,
                       int num_frames,
                       float current_volume,
                       bool is_silence) override;

  float* GetOutputBuffer() override;
  int NumOutputChannels() const override;

  bool SetOutputConfig(const AudioPostProcessor2::Config& config) override;
  int GetInputSampleRate() const override;
  bool IsRinging() override;

  // Send string |config| to post processor |name|.
  void SetPostProcessorConfig(const std::string& name,
                              const std::string& config) override;
  void SetContentType(AudioContentType content_type) override;
  void UpdatePlayoutChannel(int channel) override;

 private:
  // Note: typedef is used to silence chromium-style mandatory constructor in
  // structs.
  typedef struct {
    std::unique_ptr<AudioPostProcessor2> ptr;
    int input_frames_per_write;
    std::string name;
  } PostProcessorInfo;

  int GetRingingTimeInFrames();
  void UpdateCastVolume(float multiplier);

  std::string name_;
  int input_sample_rate_ = 0;
  int output_sample_rate_ = 0;
  int ringing_time_in_frames_ = 0;
  int silence_frames_processed_ = 0;
  double delay_s_ = 0;
  float current_multiplier_ = 0.0;
  float cast_volume_ = 0.0;
  float current_dbfs_ = 0.0;
  int num_output_channels_ = 0;
  float* output_buffer_ = nullptr;
  AlignedBuffer<float> silence_buffer_;

  // factory_ keeps shared libraries open, so it must outlive processors_.
  PostProcessorFactory factory_;

  std::vector<PostProcessorInfo> processors_;

  DISALLOW_COPY_AND_ASSIGN(PostProcessingPipelineImpl);
};

class PostProcessingPipelineFactoryImpl : public PostProcessingPipelineFactory {
 public:
  PostProcessingPipelineFactoryImpl();
  ~PostProcessingPipelineFactoryImpl() override;

  // PostProcessingPipelineFactory interface.
  std::unique_ptr<PostProcessingPipeline> CreatePipeline(
      const std::string& name,
      const base::Value* filter_description_list,
      int num_channels) override;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MIXER_POST_PROCESSING_PIPELINE_IMPL_H_
