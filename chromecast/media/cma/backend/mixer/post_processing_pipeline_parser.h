// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MIXER_POST_PROCESSING_PIPELINE_PARSER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MIXER_POST_PROCESSING_PIPELINE_PARSER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/optional.h"

namespace base {
class DictionaryValue;
class Value;
}  // namespace base

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
  const base::Value* pipeline;
  const base::Value* stream_types;
  const base::Optional<int> num_input_channels;

  StreamPipelineDescriptor(const base::Value* pipeline_in,
                           const base::Value* stream_types_in,
                           const base::Optional<int> num_input_channels_in);
  ~StreamPipelineDescriptor();
  StreamPipelineDescriptor(const StreamPipelineDescriptor& other);
  StreamPipelineDescriptor operator=(const StreamPipelineDescriptor& other) =
      delete;
};

// Helper class to parse post-processing pipeline descriptor file.
class PostProcessingPipelineParser {
 public:
  explicit PostProcessingPipelineParser(const base::FilePath& path);

  // For testing only:
  explicit PostProcessingPipelineParser(
      std::unique_ptr<base::DictionaryValue> config_dict);

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
  std::unique_ptr<base::DictionaryValue> config_dict_;
  const base::DictionaryValue* postprocessor_config_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PostProcessingPipelineParser);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MIXER_POST_PROCESSING_PIPELINE_PARSER_H_
