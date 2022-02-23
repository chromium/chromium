// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer/post_processing_pipeline_parser.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "chromecast/media/base/audio_device_ids.h"
#include "media/audio/audio_device_description.h"

namespace chromecast {
namespace media {

namespace {

const char kPostProcessorsKey[] = "postprocessors";
const char kOutputStreamsKey[] = "output_streams";
const char kMixPipelineKey[] = "mix";
const char kLinearizePipelineKey[] = "linearize";
const char kProcessorsKey[] = "processors";
const char kStreamsKey[] = "streams";
const char kNumInputChannelsKey[] = "num_input_channels";
const char kVolumeLimitsKey[] = "volume_limits";

}  // namespace

StreamPipelineDescriptor::StreamPipelineDescriptor(
    const base::Value* pipeline_in,
    const base::Value* stream_types_in,
    const absl::optional<int> num_input_channels_in,
    const base::Value* volume_limits_in)
    : pipeline(pipeline_in),
      stream_types(stream_types_in),
      num_input_channels(std::move(num_input_channels_in)),
      volume_limits(volume_limits_in) {}

StreamPipelineDescriptor::~StreamPipelineDescriptor() = default;

StreamPipelineDescriptor::StreamPipelineDescriptor(
    const StreamPipelineDescriptor& other)
    : StreamPipelineDescriptor(other.pipeline,
                               other.stream_types,
                               other.num_input_channels,
                               other.volume_limits) {}

PostProcessingPipelineParser::PostProcessingPipelineParser(
    base::Value config_dict)
    : file_path_(""), config_dict_(std::move(config_dict)) {
  postprocessor_config_ = config_dict_.FindPath(kPostProcessorsKey);
  if (!postprocessor_config_) {
    LOG(WARNING) << "No post-processor config found.";
  }
}

PostProcessingPipelineParser::PostProcessingPipelineParser(
    const base::FilePath& file_path)
    : file_path_(file_path) {
  if (!base::PathExists(file_path_)) {
    LOG(WARNING) << "No post-processing config found at " << file_path_ << ".";
    return;
  }

  JSONFileValueDeserializer deserializer(file_path_);
  int error_code = -1;
  std::string error_msg;
  auto config_dict_ptr = deserializer.Deserialize(&error_code, &error_msg);
  CHECK(config_dict_ptr) << "Invalid JSON in " << file_path_ << " error "
                         << error_code << ":" << error_msg;
  config_dict_ = base::Value(std::move(*config_dict_ptr));

  postprocessor_config_ = config_dict_.FindPath(kPostProcessorsKey);
  if (!postprocessor_config_) {
    LOG(WARNING) << "No post-processor config found.";
  }
}

PostProcessingPipelineParser::~PostProcessingPipelineParser() = default;

std::vector<StreamPipelineDescriptor>
PostProcessingPipelineParser::GetStreamPipelines() {
  std::vector<StreamPipelineDescriptor> descriptors;
  if (!postprocessor_config_) {
    return descriptors;
  }
  const base::Value* pipelines_list = postprocessor_config_->FindKeyOfType(
      kOutputStreamsKey, base::Value::Type::LIST);
  if (!pipelines_list) {
    LOG(WARNING) << "No post-processors found for streams (key = "
                 << kOutputStreamsKey
                 << ").\n No stream-specific processing will occur.";
    return descriptors;
  }
  for (const base::Value& pipeline_description_dict :
       pipelines_list->GetListDeprecated()) {
    CHECK(pipeline_description_dict.is_dict());

    const base::Value* processors_list =
        pipeline_description_dict.FindKeyOfType(kProcessorsKey,
                                                base::Value::Type::LIST);
    CHECK(processors_list);

    const base::Value* streams_list = pipeline_description_dict.FindKeyOfType(
        kStreamsKey, base::Value::Type::LIST);
    CHECK(streams_list);

    auto num_input_channels =
        pipeline_description_dict.FindIntKey(kNumInputChannelsKey);

    const base::Value* volume_limits = pipeline_description_dict.FindKeyOfType(
        kVolumeLimitsKey, base::Value::Type::DICTIONARY);

    descriptors.emplace_back(processors_list, streams_list,
                             std::move(num_input_channels), volume_limits);
  }
  return descriptors;
}

StreamPipelineDescriptor PostProcessingPipelineParser::GetMixPipeline() {
  return GetPipelineByKey(kMixPipelineKey);
}

StreamPipelineDescriptor PostProcessingPipelineParser::GetLinearizePipeline() {
  return GetPipelineByKey(kLinearizePipelineKey);
}

StreamPipelineDescriptor PostProcessingPipelineParser::GetPipelineByKey(
    const std::string& key) {
  const base::Value* stream_dict =
      postprocessor_config_ ? postprocessor_config_->FindPath(key) : nullptr;
  if (!postprocessor_config_ || !stream_dict) {
    LOG(WARNING) << "No post-processor description found for \"" << key
                 << "\" in " << file_path_ << ". Using passthrough.";
    return StreamPipelineDescriptor(nullptr, nullptr, absl::nullopt, nullptr);
  }
  const base::Value* processors_list =
      stream_dict->FindKeyOfType(kProcessorsKey, base::Value::Type::LIST);
  CHECK(processors_list);

  const base::Value* streams_list =
      stream_dict->FindKeyOfType(kStreamsKey, base::Value::Type::LIST);

  const base::Value* volume_limits = stream_dict->FindKeyOfType(
      kVolumeLimitsKey, base::Value::Type::DICTIONARY);

  return StreamPipelineDescriptor(processors_list, streams_list,
                                  stream_dict->FindIntKey(kNumInputChannelsKey),
                                  volume_limits);
}

base::FilePath PostProcessingPipelineParser::GetFilePath() const {
  return file_path_;
}

}  // namespace media
}  // namespace chromecast
