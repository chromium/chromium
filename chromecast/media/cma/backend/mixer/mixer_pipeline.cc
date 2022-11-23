// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer/mixer_pipeline.h"

#include <algorithm>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "chromecast/media/base/audio_device_ids.h"
#include "chromecast/media/cma/backend/mixer/filter_group.h"
#include "chromecast/media/cma/backend/mixer/post_processing_pipeline_impl.h"
#include "chromecast/media/cma/backend/mixer/post_processing_pipeline_parser.h"
#include "chromecast/public/media/audio_post_processor2_shlib.h"
#include "media/audio/audio_device_description.h"

namespace chromecast {
namespace media {

namespace {

bool IsOutputDeviceId(const std::string& device) {
  return device == ::media::AudioDeviceDescription::kDefaultDeviceId ||
         device == ::media::AudioDeviceDescription::kCommunicationsDeviceId ||
         device == kLocalAudioDeviceId || device == kAlarmAudioDeviceId ||
         device == kNoDelayDeviceId || device == kLowLatencyDeviceId ||
         device == kPlatformAudioDeviceId /* e.g. bluetooth and aux */ ||
         device == kTtsAudioDeviceId || device == kBypassAudioDeviceId;
}

}  // namespace

// static
std::unique_ptr<MixerPipeline> MixerPipeline::CreateMixerPipeline(
    PostProcessingPipelineParser* config,
    PostProcessingPipelineFactory* factory,
    int expected_input_channels) {
  std::unique_ptr<MixerPipeline> mixer_pipeline(new MixerPipeline());
  if (mixer_pipeline->BuildPipeline(config, factory, expected_input_channels)) {
    return mixer_pipeline;
  }
  return nullptr;
}

MixerPipeline::MixerPipeline() = default;
MixerPipeline::~MixerPipeline() = default;

bool MixerPipeline::BuildPipeline(PostProcessingPipelineParser* config,
                                  PostProcessingPipelineFactory* factory,
                                  int expected_input_channels) {
  DCHECK(config);
  DCHECK(factory);
  int mix_group_input_channels = -1;

  // Create "stream" processor groups:
  for (auto& stream_pipeline : config->GetStreamPipelines()) {
    const base::Value::List& device_ids =
        stream_pipeline.stream_types->GetList();
    int input_channels = (stream_pipeline.num_input_channels.has_value()
                              ? stream_pipeline.num_input_channels.value()
                              : expected_input_channels);
    DCHECK(!device_ids.empty());
    DCHECK(device_ids[0].is_string());
    const std::string& name = device_ids[0].GetString();
    LOG(INFO) << input_channels << " input channels to '" << name << "' group";

    filter_groups_.push_back(std::make_unique<FilterGroup>(
        input_channels, name, stream_pipeline.prerender_pipeline.Clone(),
        &stream_pipeline.pipeline, factory, stream_pipeline.volume_limits));

    if (!SetGroupDeviceIds(stream_pipeline.stream_types,
                           filter_groups_.back().get())) {
      return false;
    }

    mix_group_input_channels =
        std::max(mix_group_input_channels,
                 filter_groups_.back()->GetOutputChannelCount());
  }

  if (mix_group_input_channels == -1) {
    mix_group_input_channels = expected_input_channels;
  }

  // Create "mix" processor group:
  const auto mix_pipeline = config->GetMixPipeline();
  if (mix_pipeline.num_input_channels.has_value()) {
    mix_group_input_channels = mix_pipeline.num_input_channels.value();
  }
  LOG(INFO) << mix_group_input_channels << " input channels to 'mix' group";
  auto mix_filter = std::make_unique<FilterGroup>(
      mix_group_input_channels, "mix", mix_pipeline.prerender_pipeline.Clone(),
      &mix_pipeline.pipeline, factory, mix_pipeline.volume_limits);
  for (std::unique_ptr<FilterGroup>& group : filter_groups_) {
    mix_filter->AddMixedInput(group.get());
  }
  if (!SetGroupDeviceIds(mix_pipeline.stream_types, mix_filter.get())) {
    return false;
  }
  loopback_output_group_ = mix_filter.get();
  filter_groups_.push_back(std::move(mix_filter));

  // Create "linearize" processor group:
  const auto linearize_pipeline = config->GetLinearizePipeline();
  int linearize_group_input_channels =
      loopback_output_group_->GetOutputChannelCount();
  if (linearize_pipeline.num_input_channels.has_value()) {
    linearize_group_input_channels =
        linearize_pipeline.num_input_channels.value();
  }
  LOG(INFO) << linearize_group_input_channels
            << " input channels to 'linearize' group";
  filter_groups_.push_back(std::make_unique<FilterGroup>(
      linearize_group_input_channels, "linearize",
      linearize_pipeline.prerender_pipeline.Clone(),
      &linearize_pipeline.pipeline, factory, linearize_pipeline.volume_limits));
  output_group_ = filter_groups_.back().get();
  output_group_->AddMixedInput(loopback_output_group_);
  if (!SetGroupDeviceIds(linearize_pipeline.stream_types, output_group_)) {
    return false;
  }

  // If no default group is provided, use the "mix" group.
  if (stream_sinks_.find(::media::AudioDeviceDescription::kDefaultDeviceId) ==
      stream_sinks_.end()) {
    stream_sinks_[::media::AudioDeviceDescription::kDefaultDeviceId] =
        loopback_output_group_;
  }

  return true;
}

bool MixerPipeline::SetGroupDeviceIds(const base::Value* ids,
                                      FilterGroup* filter_group) {
  if (!ids) {
    return true;
  }
  DCHECK(filter_group);
  DCHECK(ids->is_list());

  for (const base::Value& stream_type_val : ids->GetList()) {
    DCHECK(stream_type_val.is_string());
    const std::string& stream_type = stream_type_val.GetString();
    if (!IsOutputDeviceId(stream_type)) {
      LOG(ERROR) << stream_type
                 << " is not a stream type. Stream types are listed "
                 << "in chromecast/media/base/audio_device_ids.cc and "
                 << "media/audio/audio_device_description.cc";
      return false;
    }
    if (stream_sinks_.find(stream_type) != stream_sinks_.end()) {
      LOG(ERROR) << "Multiple instances of stream type '" << stream_type
                 << "' in cast_audio.json";
      return false;
    }
    stream_sinks_[stream_type] = filter_group;
    filter_group->AddStreamType(stream_type);
  }
  return true;
}

void MixerPipeline::Initialize(int output_samples_per_second,
                               int frames_per_write) {
  // The output group will recursively set the sample rate of all other
  // FilterGroups.
  AudioPostProcessor2::Config config;
  config.output_sample_rate = output_samples_per_second;
  config.system_output_sample_rate = output_samples_per_second;
  config.output_frames_per_write = frames_per_write;
  output_group_->Initialize(config);
  output_group_->PrintTopology();
}

FilterGroup* MixerPipeline::GetInputGroup(const std::string& device_id) {
  auto got = stream_sinks_.find(device_id);
  if (got != stream_sinks_.end()) {
    return got->second;
  }

  return stream_sinks_[::media::AudioDeviceDescription::kDefaultDeviceId];
}

void MixerPipeline::MixAndFilter(
    int frames_per_write,
    MediaPipelineBackend::AudioDecoder::RenderingDelay rendering_delay) {
  output_group_->MixAndFilter(frames_per_write, rendering_delay);
}

float* MixerPipeline::GetLoopbackOutput() {
  return loopback_output_group_->GetOutputBuffer();
}

float* MixerPipeline::GetOutput() {
  return output_group_->GetOutputBuffer();
}

int MixerPipeline::GetLoopbackChannelCount() const {
  return loopback_output_group_->GetOutputChannelCount();
}

int MixerPipeline::GetOutputChannelCount() const {
  return output_group_->GetOutputChannelCount();
}

int64_t MixerPipeline::GetPostLoopbackRenderingDelayMicroseconds() const {
  return output_group_->GetRenderingDelayMicroseconds();
}

void MixerPipeline::SetPostProcessorConfig(const std::string& name,
                                           const std::string& config) {
  for (auto&& filter_group : filter_groups_) {
    filter_group->SetPostProcessorConfig(name, config);
  }
}

bool MixerPipeline::IsRinging() const {
  for (auto& filter_group : filter_groups_) {
    if (filter_group->IsRinging()) {
      return true;
    }
  }
  return false;
}

}  // namespace media
}  // namespace chromecast
