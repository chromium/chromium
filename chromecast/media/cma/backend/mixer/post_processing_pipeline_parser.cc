// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer/post_processing_pipeline_parser.h"

#include <utility>

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "chromecast/media/base/audio_device_ids.h"
#include "media/audio/audio_device_description.h"

namespace chromecast {
namespace media {

namespace {

const char kLinearizePipelineKey[] = "linearize";
const char kMixPipelineKey[] = "mix";
const char kNameKey[] = "name";
const char kNumInputChannelsKey[] = "num_input_channels";
const char kOutputStreamsKey[] = "output_streams";
const char kPostProcessorsKey[] = "postprocessors";
const char kProcessorsKey[] = "processors";
const char kRenderNameTag[] = "render";
const char kStreamsKey[] = "streams";
const char kVolumeLimitsKey[] = "volume_limits";

void SplitPipeline(const base::Value::List& processors_list,
                   base::Value::List& prerender_pipeline,
                   base::Value::List& postrender_pipeline) {
  bool has_render = false;
  for (const base::Value& processor_description_value : processors_list) {
    DCHECK(processor_description_value.is_dict());
    const std::string* name =
        processor_description_value.GetDict().FindString(kNameKey);
    if (name && *name == kRenderNameTag) {
      has_render = true;
      break;
    }
  }

  bool is_prerender = has_render;

  for (const base::Value& processor_description_dict : processors_list) {
    const std::string* name =
        processor_description_dict.GetDict().FindString(kNameKey);
    if (name && *name == kRenderNameTag) {
      is_prerender = false;
      continue;
    }

    if (is_prerender) {
      prerender_pipeline.Append(processor_description_dict.Clone());
    } else {
      postrender_pipeline.Append(processor_description_dict.Clone());
    }
  }
}

}  // namespace

StreamPipelineDescriptor::StreamPipelineDescriptor(
    base::Value prerender_pipeline_in,
    base::Value pipeline_in,
    const base::Value* stream_types_in,
    const std::optional<int> num_input_channels_in,
    const base::Value* volume_limits_in)
    : prerender_pipeline(std::move(prerender_pipeline_in)),
      pipeline(std::move(pipeline_in)),
      stream_types(stream_types_in),
      num_input_channels(std::move(num_input_channels_in)),
      volume_limits(volume_limits_in) {}

StreamPipelineDescriptor::~StreamPipelineDescriptor() = default;

StreamPipelineDescriptor::StreamPipelineDescriptor(
    StreamPipelineDescriptor&& other) = default;
StreamPipelineDescriptor& StreamPipelineDescriptor::operator=(
    StreamPipelineDescriptor&& other) = default;

PostProcessingPipelineParser::PostProcessingPipelineParser(
    base::Value config_dict)
    : file_path_(""), config_dict_(std::move(config_dict).TakeDict()) {
  postprocessor_config_ = config_dict_.FindDict(kPostProcessorsKey);
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
  config_dict_ = std::move(config_dict_ptr->GetDict());

  postprocessor_config_ = config_dict_.FindDict(kPostProcessorsKey);
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
  const base::Value::List* pipelines_list =
      postprocessor_config_->FindList(kOutputStreamsKey);
  if (!pipelines_list) {
    LOG(WARNING) << "No post-processors found for streams (key = "
                 << kOutputStreamsKey
                 << ").\n No stream-specific processing will occur.";
    return descriptors;
  }
  for (const base::Value& pipeline_description_val : *pipelines_list) {
    CHECK(pipeline_description_val.is_dict());
    const base::Value::Dict& pipeline_description_dict =
        pipeline_description_val.GetDict();

    const base::Value::List* processors_list =
        pipeline_description_dict.FindList(kProcessorsKey);
    CHECK(processors_list);

    base::Value::List prerender_pipeline;
    base::Value::List postrender_pipeline;
    SplitPipeline(*processors_list, prerender_pipeline, postrender_pipeline);

    const base::Value* streams_list =
        pipeline_description_dict.Find(kStreamsKey);
    CHECK(streams_list && streams_list->is_list());

    auto num_input_channels =
        pipeline_description_dict.FindInt(kNumInputChannelsKey);

    const base::Value* volume_limits =
        pipeline_description_dict.Find(kVolumeLimitsKey);
    CHECK(!volume_limits || volume_limits->is_list());

    descriptors.emplace_back(base::Value(std::move(prerender_pipeline)),
                             base::Value(std::move(postrender_pipeline)),
                             streams_list, std::move(num_input_channels),
                             volume_limits);
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
  const base::Value* stream_value =
      postprocessor_config_ ? postprocessor_config_->FindByDottedPath(key)
                            : nullptr;
  if (!postprocessor_config_ || !stream_value) {
    LOG(WARNING) << "No post-processor description found for \"" << key
                 << "\" in " << file_path_ << ". Using passthrough.";
    return StreamPipelineDescriptor(base::Value(base::Value::Type::LIST),
                                    base::Value(base::Value::Type::LIST),
                                    nullptr, std::nullopt, nullptr);
  }

  const base::Value::Dict& stream_dict = stream_value->GetDict();
  const base::Value::List* processors_list =
      stream_dict.FindList(kProcessorsKey);
  CHECK(processors_list);

  base::Value::List prerender_pipeline;
  base::Value::List postrender_pipeline;
  SplitPipeline(*processors_list, prerender_pipeline, postrender_pipeline);

  const base::Value* streams_list = stream_dict.Find(kStreamsKey);
  if (streams_list && !streams_list->is_list()) {
    streams_list = nullptr;
  }

  const base::Value* volume_limits = stream_dict.Find(kVolumeLimitsKey);
  if (volume_limits && !volume_limits->is_dict()) {
    volume_limits = nullptr;
  }

  return StreamPipelineDescriptor(
      base::Value(std::move(prerender_pipeline)),
      base::Value(std::move(postrender_pipeline)), streams_list,
      stream_dict.FindInt(kNumInputChannelsKey), volume_limits);
}

base::FilePath PostProcessingPipelineParser::GetFilePath() const {
  return file_path_;
}

}  // namespace media
}  // namespace chromecast
