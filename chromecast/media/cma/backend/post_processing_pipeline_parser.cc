// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/post_processing_pipeline_parser.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/values.h"
#include "chromecast/base/serializers.h"
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

}  // namespace

StreamPipelineDescriptor::StreamPipelineDescriptor(
    const base::ListValue* pipeline_in,
    const base::flat_set<std::string>& stream_types_in)
    : pipeline(pipeline_in), stream_types(stream_types_in) {}

StreamPipelineDescriptor::~StreamPipelineDescriptor() = default;

StreamPipelineDescriptor::StreamPipelineDescriptor(
    const StreamPipelineDescriptor& other)
    : StreamPipelineDescriptor(other.pipeline, other.stream_types) {}

PostProcessingPipelineParser::PostProcessingPipelineParser(
    std::unique_ptr<base::DictionaryValue> config_dict)
    : file_path_(""), config_dict_(std::move(config_dict)) {
  CHECK(config_dict_) << "Invalid JSON";
  if (!config_dict_->GetDictionary(kPostProcessorsKey,
                                   &postprocessor_config_)) {
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

  config_dict_ =
      base::DictionaryValue::From(DeserializeJsonFromFile(file_path_));
  CHECK(config_dict_) << "Invalid JSON in " << file_path_;

  if (!config_dict_->GetDictionary(kPostProcessorsKey,
                                   &postprocessor_config_)) {
    LOG(WARNING) << "No post-processor config found.";
  }
}

PostProcessingPipelineParser::~PostProcessingPipelineParser() = default;

std::vector<StreamPipelineDescriptor>
PostProcessingPipelineParser::GetStreamPipelines() {
  std::vector<StreamPipelineDescriptor> descriptors;
  const base::ListValue* pipelines_list;
  if (!postprocessor_config_ ||
      !postprocessor_config_->GetList(kOutputStreamsKey, &pipelines_list)) {
    LOG(WARNING) << "No post-processors found for streams (key = "
                 << kOutputStreamsKey
                 << ").\n No stream-specific processing will occur.";
    return descriptors;
  }
  for (size_t i = 0; i < pipelines_list->GetSize(); ++i) {
    const base::DictionaryValue* pipeline_description_dict;
    CHECK(pipelines_list->GetDictionary(i, &pipeline_description_dict));

    const base::ListValue* processors_list;
    CHECK(pipeline_description_dict->GetList(kProcessorsKey, &processors_list));

    const base::ListValue* streams_list;
    CHECK(pipeline_description_dict->GetList(kStreamsKey, &streams_list));
    base::flat_set<std::string> streams_set;
    for (size_t stream = 0; stream < streams_list->GetSize(); ++stream) {
      std::string stream_name;
      CHECK(streams_list->GetString(stream, &stream_name));
      CHECK(streams_set.insert(stream_name).second)
          << "Duplicate stream type: " << stream_name;
    }

    descriptors.emplace_back(processors_list, std::move(streams_set));
  }
  return descriptors;
}

const base::ListValue* PostProcessingPipelineParser::GetMixPipeline() {
  return GetPipelineByKey(kMixPipelineKey);
}

const base::ListValue* PostProcessingPipelineParser::GetLinearizePipeline() {
  return GetPipelineByKey(kLinearizePipelineKey);
}

const base::ListValue* PostProcessingPipelineParser::GetPipelineByKey(
    const std::string& key) {
  const base::DictionaryValue* stream_dict;
  if (!postprocessor_config_ ||
      !postprocessor_config_->GetDictionary(key, &stream_dict)) {
    LOG(WARNING) << "No post-processor description found for \"" << key
                 << "\" in " << file_path_ << ". Using passthrough.";
    return nullptr;
  }
  const base::ListValue* out_list;
  CHECK(stream_dict->GetList(kProcessorsKey, &out_list));

  return out_list;
}

base::FilePath PostProcessingPipelineParser::GetFilePath() const {
  return file_path_;
}

}  // namespace media
}  // namespace chromecast
