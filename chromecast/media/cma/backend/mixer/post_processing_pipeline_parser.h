// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MIXER_POST_PROCESSING_PIPELINE_PARSER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MIXER_POST_PROCESSING_PIPELINE_PARSER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/values.h"

namespace chromecast {
namespace media {

// Helper class to hold information about a stream pipeline.
struct StreamPipelineDescriptor {
  // The format for pipeline is:
  // [ {"processor": "PATH_TO_SHARED_OBJECT",
  //    "config": "CONFIGURATION_STRING"},
  //   {"processor": "PATH_TO_SHARED_OBJECT",
  //    "config": "CONFIGURATION_STRING"},
  //    ... ]
  base::Value prerender_pipeline;
  base::Value pipeline;
  const base::Value* stream_types;
  std::optional<int> num_input_channels;
  const base::Value* volume_limits;

  StreamPipelineDescriptor(base::Value prerender_pipeline_in,
                           base::Value pipeline_in,
                           const base::Value* stream_types_in,
                           const std::optional<int> num_input_channels_in,
                           const base::Value* volume_limits_in);
  ~StreamPipelineDescriptor();
  StreamPipelineDescriptor(StreamPipelineDescriptor&& other);
  StreamPipelineDescriptor& operator=(StreamPipelineDescriptor&& other);

  StreamPipelineDescriptor(const StreamPipelineDescriptor&) = delete;
  StreamPipelineDescriptor& operator=(const StreamPipelineDescriptor&) = delete;
};

// Helper class to parse post-processing pipeline descriptor file.
class PostProcessingPipelineParser {
 public:
  explicit PostProcessingPipelineParser(const base::FilePath& path);

  // For testing only:
  explicit PostProcessingPipelineParser(base::Value config_dict);

  PostProcessingPipelineParser(const PostProcessingPipelineParser&) = delete;
  PostProcessingPipelineParser& operator=(const PostProcessingPipelineParser&) =
      delete;

  ~PostProcessingPipelineParser();

  std::vector<StreamPipelineDescriptor> GetStreamPipelines();

  // Gets the list of processors for the mix/linearize stages.
  // Same format as StreamPipelineDescriptor.pipeline
  StreamPipelineDescriptor GetMixPipeline();
  StreamPipelineDescriptor GetLinearizePipeline();

  // Returns the file path used to load this object.
  base::FilePath GetFilePath() const;

 private:
  StreamPipelineDescriptor GetPipelineByKey(const std::string& key);

  const base::FilePath file_path_;
  base::Value::Dict config_dict_;
  const base::Value::Dict* postprocessor_config_ = nullptr;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MIXER_POST_PROCESSING_PIPELINE_PARSER_H_
