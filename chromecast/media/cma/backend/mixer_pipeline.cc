// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer_pipeline.h"

#include <utility>

#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "chromecast/media/base/audio_device_ids.h"
#include "chromecast/media/cma/backend/filter_group.h"
#include "chromecast/media/cma/backend/post_processing_pipeline_impl.h"
#include "chromecast/media/cma/backend/post_processing_pipeline_parser.h"
#include "media/audio/audio_device_description.h"

namespace chromecast {
namespace media {

namespace {

const int kNumInputChannels = 2;

bool IsOutputDeviceId(const std::string& device) {
  return device == ::media::AudioDeviceDescription::kDefaultDeviceId ||
         device == ::media::AudioDeviceDescription::kCommunicationsDeviceId ||
         device == kLocalAudioDeviceId || device == kAlarmAudioDeviceId ||
         device == kPlatformAudioDeviceId /* e.g. bluetooth and aux */ ||
         device == kTtsAudioDeviceId || device == kBypassAudioDeviceId;
}

std::unique_ptr<FilterGroup> CreateFilterGroup(
    FilterGroup::GroupType type,
    int input_channels,
    const std::string& name,
    const base::ListValue* filter_list,
    const base::flat_set<std::string>& device_ids,
    const std::vector<FilterGroup*>& mixed_inputs,
    PostProcessingPipelineFactory* ppp_factory) {
  DCHECK(ppp_factory);
  auto pipeline =
      ppp_factory->CreatePipeline(name, filter_list, input_channels);
  return std::make_unique<FilterGroup>(input_channels, type, name,
                                       std::move(pipeline), device_ids,
                                       mixed_inputs);
}

}  // namespace

// static
std::unique_ptr<MixerPipeline> MixerPipeline::CreateMixerPipeline(
    PostProcessingPipelineParser* config,
    PostProcessingPipelineFactory* factory) {
  std::unique_ptr<MixerPipeline> mixer_pipeline(new MixerPipeline);

  if (mixer_pipeline->BuildPipeline(config, factory)) {
    return mixer_pipeline;
  }
  return nullptr;
}

MixerPipeline::MixerPipeline() = default;
MixerPipeline::~MixerPipeline() = default;

bool MixerPipeline::BuildPipeline(PostProcessingPipelineParser* config,
                                  PostProcessingPipelineFactory* factory) {
  DCHECK(config);
  DCHECK(factory);
  base::flat_set<std::string> used_streams;
  for (auto& stream_pipeline : config->GetStreamPipelines()) {
    const auto& device_ids = stream_pipeline.stream_types;
    for (const std::string& stream_type : device_ids) {
      if (!IsOutputDeviceId(stream_type)) {
        LOG(ERROR) << stream_type
                   << " is not a stream type. Stream types are listed "
                   << "in chromecast/media/base/audio_device_ids.cc and "
                   << "media/audio/audio_device_description.cc";
        return false;
      }
      if (!used_streams.insert(stream_type).second) {
        LOG(ERROR) << "Multiple instances of stream type '" << stream_type
                   << "' in " << config->GetFilePath() << ".";
        return false;
      }
      filter_groups_.push_back(CreateFilterGroup(
          FilterGroup::GroupType::kStream, kNumInputChannels,
          *device_ids.begin() /* name */, stream_pipeline.pipeline, device_ids,
          std::vector<FilterGroup*>() /* mixed_inputs */, factory));
      if (device_ids.find(::media::AudioDeviceDescription::kDefaultDeviceId) !=
          device_ids.end()) {
        default_stream_group_ = filter_groups_.back().get();
      }
    }
  }

  if (!filter_groups_.empty()) {
    std::vector<FilterGroup*> filter_group_ptrs(filter_groups_.size());
    int mix_group_input_channels = filter_groups_[0]->GetOutputChannelCount();
    for (size_t i = 0; i < filter_groups_.size(); ++i) {
      if (mix_group_input_channels !=
          filter_groups_[i]->GetOutputChannelCount()) {
        LOG(ERROR)
            << "All output stream mixers must have the same number of channels"
            << filter_groups_[i]->name() << " has "
            << filter_groups_[i]->GetOutputChannelCount() << " but others have "
            << mix_group_input_channels;
        return false;
      }
      filter_group_ptrs[i] = filter_groups_[i].get();
    }

    filter_groups_.push_back(CreateFilterGroup(
        FilterGroup::GroupType::kFinalMix, mix_group_input_channels, "mix",
        config->GetMixPipeline(),
        base::flat_set<std::string>() /* device_ids */, filter_group_ptrs,
        factory));
  } else {
    // Mix group directly mixes all inputs.
    std::string kDefaultDeviceId =
        ::media::AudioDeviceDescription::kDefaultDeviceId;
    filter_groups_.push_back(CreateFilterGroup(
        FilterGroup::GroupType::kFinalMix, kNumInputChannels, "mix",
        config->GetMixPipeline(),
        base::flat_set<std::string>({kDefaultDeviceId}),
        std::vector<FilterGroup*>() /* mixed_inputs */, factory));
    default_stream_group_ = filter_groups_.back().get();
  }

  loopback_output_group_ = filter_groups_.back().get();

  filter_groups_.push_back(CreateFilterGroup(
      FilterGroup::GroupType::kLinearize,
      loopback_output_group_->GetOutputChannelCount(), "linearize",
      config->GetLinearizePipeline(),
      base::flat_set<std::string>() /* device_ids */,
      std::vector<FilterGroup*>({loopback_output_group_}), factory));
  output_group_ = filter_groups_.back().get();

  LOG(INFO) << "PostProcessor configuration:";
  if (default_stream_group_ == loopback_output_group_) {
    LOG(INFO) << "Stream layer: none";
  } else {
    LOG(INFO) << "Stream layer: "
              << default_stream_group_->GetOutputChannelCount() << " channels";
  }
  LOG(INFO) << "Mix filter: " << loopback_output_group_->GetOutputChannelCount()
            << " channels";
  LOG(INFO) << "Linearize filter: " << output_group_->GetOutputChannelCount()
            << " channels";

  return true;
}

void MixerPipeline::Initialize(int output_samples_per_second_) {
  for (auto&& filter_group : filter_groups_) {
    filter_group->Initialize(output_samples_per_second_);
  }
}

FilterGroup* MixerPipeline::GetInputGroup(const std::string& device_id) {
  for (auto&& filter_group : filter_groups_) {
    if (filter_group->CanProcessInput(device_id)) {
      return filter_group.get();
      break;
    }
  }

  if (default_stream_group_) {
    return default_stream_group_;
  }

  NOTREACHED() << "Could not find a filter group to re-attach " << device_id;
  return nullptr;
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

void MixerPipeline::SetMixToMono(bool mix_to_mono) {
  loopback_output_group_->SetMixToMono(mix_to_mono);
}

void MixerPipeline::SetPlayoutChannel(int playout_channel) {
  for (auto& filter_group : filter_groups_) {
    filter_group->UpdatePlayoutChannel(playout_channel);
  }
}

}  // namespace media
}  // namespace chromecast
