// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer/filter_group.h"

#include <algorithm>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromecast/media/audio/audio_log.h"
#include "chromecast/media/audio/interleaved_channel_mixer.h"
#include "chromecast/media/cma/backend/mixer/channel_layout.h"
#include "chromecast/media/cma/backend/mixer/mixer_input.h"
#include "chromecast/media/cma/backend/mixer/post_processing_pipeline.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_sample_types.h"
#include "media/base/vector_math.h"

namespace chromecast {
namespace media {

namespace {

bool ParseVolumeLimit(const base::Value::Dict* dict, float* min, float* max) {
  auto min_value = dict->FindDouble("min");
  auto max_value = dict->FindDouble("max");
  if (!min_value && !max_value) {
    return false;
  }
  *min = 0.0f;
  *max = 1.0f;
  if (min_value) {
    *min = std::clamp(static_cast<float>(min_value.value()), 0.0f, 1.0f);
  }
  if (max_value) {
    *max = std::clamp(static_cast<float>(max_value.value()), *min, 1.0f);
  }
  return true;
}

}  // namespace

FilterGroup::GroupInput::GroupInput(
    FilterGroup* group,
    std::unique_ptr<InterleavedChannelMixer> channel_mixer)
    : group(group), channel_mixer(std::move(channel_mixer)) {}

FilterGroup::GroupInput::GroupInput(GroupInput&& other) = default;
FilterGroup::GroupInput::~GroupInput() = default;

FilterGroup::FilterGroup(int num_channels,
                         std::string name,
                         base::Value prerender_filter_list,
                         const base::Value* filter_list,
                         PostProcessingPipelineFactory* ppp_factory,
                         const base::Value* volume_limits)
    : num_channels_(num_channels),
      name_(std::move(name)),
      prerender_filter_list_(std::move(prerender_filter_list)),
      ppp_factory_(ppp_factory),
      tag_(base::MakeRefCounted<FilterGroupTag>()),
      post_processing_pipeline_(
          ppp_factory_->CreatePipeline(name_, filter_list, num_channels_)) {
  LOG(INFO) << "Done creating postrender pipeline for " << name_;
  ParseVolumeLimits(volume_limits);
}

FilterGroup::~FilterGroup() = default;

std::unique_ptr<PostProcessingPipeline> FilterGroup::CreatePrerenderPipeline(
    int num_channels) {
  ++prerender_creation_count_;
  LOG(INFO) << "Creating prerender pipeline for " << name_;
  auto pipeline = ppp_factory_->CreatePipeline(
      "prerender_" + name_ + base::NumberToString(prerender_creation_count_),
      &prerender_filter_list_, num_channels);
  for (const auto& config : post_processing_configs_) {
    LOG(INFO) << "Setting prerender post processing config for " << config.first
              << " to " << config.second;
    pipeline->SetPostProcessorConfig(config.first, config.second);
  }
  LOG(INFO) << "Done creating prerender pipeline for " << name_;
  return pipeline;
}

void FilterGroup::AddMixedInput(FilterGroup* input) {
  // Channel mixers are created in Initialize().
  mixed_inputs_.emplace_back(input, std::unique_ptr<InterleavedChannelMixer>());
  DCHECK_EQ(input->GetOutputChannelCount(), num_channels_);
}

void FilterGroup::AddStreamType(const std::string& stream_type) {
  stream_types_.push_back(stream_type);
}

void FilterGroup::Initialize(const AudioPostProcessor2::Config& output_config) {
  output_config_ = output_config;

  CHECK(post_processing_pipeline_->SetOutputConfig(output_config_));
  input_samples_per_second_ = post_processing_pipeline_->GetInputSampleRate();
  input_frames_per_write_ = output_config_.output_frames_per_write *
                            input_samples_per_second_ /
                            output_config_.output_sample_rate;
  DCHECK_EQ(input_frames_per_write_ * output_config_.output_sample_rate,
            output_config_.output_frames_per_write * input_samples_per_second_)
      << "Unable to produce stable buffer sizes for resampling rate "
      << input_samples_per_second_ << " : "
      << output_config_.output_sample_rate;

  AudioPostProcessor2::Config input_config = output_config;
  input_config.output_sample_rate = input_samples_per_second_;
  input_config.output_frames_per_write = input_frames_per_write_;

  for (auto& input : mixed_inputs_) {
    input.group->Initialize(input_config);
    input.channel_mixer = std::make_unique<InterleavedChannelMixer>(
        mixer::GuessChannelLayout(input.group->GetOutputChannelCount()),
        input.group->GetOutputChannelCount(),
        mixer::GuessChannelLayout(num_channels_), num_channels_,
        input_frames_per_write_);
  }
  post_processing_pipeline_->SetContentType(content_type_);
  active_inputs_.clear();
  ResizeBuffers();

  // Run a buffer of 0's to initialize rendering delay.
  std::fill_n(interleaved_.data(), interleaved_.size(), 0.0f);
  post_processing_pipeline_->ProcessFrames(
      interleaved_.data(), input_frames_per_write_, last_volume_, last_volume_,
      true /* is_silence */);
  delay_seconds_ = post_processing_pipeline_->GetDelaySeconds();
}

void FilterGroup::ParseVolumeLimits(const base::Value* volume_limits) {
  if (!volume_limits) {
    return;
  }

  DCHECK(volume_limits->is_dict());
  // Get default limits.
  if (ParseVolumeLimit(&volume_limits->GetDict(), &default_volume_min_,
                       &default_volume_max_)) {
    AUDIO_LOG(INFO) << "Default volume limits for '" << name_ << "' group: ["
                    << default_volume_min_ << ", " << default_volume_max_
                    << "]";
  }

  float min, max;
  for (const auto item : volume_limits->GetDict()) {
    if (item.second.is_dict() &&
        ParseVolumeLimit(&item.second.GetDict(), &min, &max)) {
      AUDIO_LOG(INFO) << "Volume limits for device ID '" << item.first
                      << "' = [" << min << ", " << max << "]";
      volume_limits_.insert({item.first, {min, max}});
    }
  }
}

void FilterGroup::AddInput(MixerInput* input) {
  active_inputs_.insert(input);

  auto it = volume_limits_.find(input->device_id());
  if (it != volume_limits_.end()) {
    input->SetVolumeLimits(it->second.first, it->second.second);
    return;
  }

  input->SetVolumeLimits(default_volume_min_, default_volume_max_);
}

void FilterGroup::RemoveInput(MixerInput* input) {
  active_inputs_.erase(input);
}

float FilterGroup::MixAndFilter(
    int num_output_frames,
    MediaPipelineBackend::AudioDecoder::RenderingDelay rendering_delay) {
  DCHECK_NE(output_config_.output_sample_rate, 0);
  DCHECK_EQ(num_output_frames, output_config_.output_frames_per_write);

  float volume = 0.0f;
  float target_volume = 0.0f;
  AudioContentType content_type = static_cast<AudioContentType>(-1);

  rendering_delay.delay_microseconds += GetRenderingDelayMicroseconds();
  rendering_delay_to_output_ = rendering_delay;

  // Recursively mix inputs.
  for (const auto& filter_group : mixed_inputs_) {
    volume = std::max(volume, filter_group.group->MixAndFilter(
                                  input_frames_per_write_, rendering_delay));
    target_volume =
        std::max(target_volume, filter_group.group->target_volume());
    content_type = std::max(content_type, filter_group.group->content_type());
  }

  // Render direct inputs.
  for (MixerInput* input : active_inputs_) {
    if (input->Render(input_frames_per_write_, rendering_delay)) {
      volume = std::max(volume, input->InstantaneousVolume());
      target_volume = std::max(target_volume, input->TargetVolume());
      content_type = std::max(content_type, input->content_type());
    }
  }

  // |volume| can only be 0 if no |mixed_inputs_| or |active_inputs_| have data.
  // This is true because FilterGroup can only return 0 if:
  // a) It has no data and its PostProcessorPipeline is not ringing.
  //    (early return, below) or
  // b) The output volume is 0 and has NEVER been non-zero,
  //    since FilterGroup will use last_volume_ if volume is 0.
  //    In this case, there was never any data in the pipeline.
  if (active_inputs_.empty() && volume == 0.0f &&
      !post_processing_pipeline_->IsRinging()) {
    last_volume_ = 0.0f;
    output_buffer_ = interleaved_.data();
    ZeroOutputBufferIfNeeded();
    return 0.0f;  // Output will be silence, no need to mix/process.
  }

  output_frames_zeroed_ = 0;
  std::fill_n(interleaved_.data(), interleaved_.size(), 0.0f);

  // Mix FilterGroups
  for (const auto& input : mixed_inputs_) {
    if (input.group->last_volume() > 0.0f) {
      float* buffer = input.channel_mixer->Transform(
          input.group->GetOutputBuffer(), input_frames_per_write_);
      for (int i = 0; i < input_frames_per_write_ * num_channels_; ++i) {
        interleaved_[i] += buffer[i];
      }
    }
  }

  // Mix direct inputs.
  for (MixerInput* input : active_inputs_) {
    if (input->has_render_output()) {
      float* buffer = input->RenderedAudioBuffer();
      for (int i = 0; i < input_frames_per_write_ * num_channels_; ++i) {
        interleaved_[i] += buffer[i];
      }
    }
  }

  // Allow paused streams to "ring out" at the last valid volume.
  // If the stream volume is actually 0, this doesn't matter, since the
  // data is 0's anyway.
  bool is_silence = (volume == 0.0f);
  if (!is_silence) {
    last_volume_ = volume;
    target_volume_ = target_volume;
    DCHECK_NE(-1, static_cast<int>(content_type))
        << "Got frames without content type.";
    if (content_type != content_type_) {
      content_type_ = content_type;
      post_processing_pipeline_->SetContentType(content_type_);
    }
  }

  post_processing_pipeline_->ProcessFrames(
      interleaved_.data(), input_frames_per_write_, last_volume_,
      target_volume_, is_silence);
  delay_seconds_ = post_processing_pipeline_->GetDelaySeconds();
  output_buffer_ = post_processing_pipeline_->GetOutputBuffer();
  return last_volume_;
}

float* FilterGroup::GetOutputBuffer() {
  DCHECK(output_buffer_);
  return output_buffer_;
}

int64_t FilterGroup::GetRenderingDelayMicroseconds() {
  if (output_config_.output_sample_rate == 0) {
    return 0;
  }
  return delay_seconds_ * base::Time::kMicrosecondsPerSecond;
}

MediaPipelineBackend::AudioDecoder::RenderingDelay
FilterGroup::GetRenderingDelayToOutput() {
  return rendering_delay_to_output_;
}

int FilterGroup::GetOutputChannelCount() const {
  return post_processing_pipeline_->NumOutputChannels();
}

void FilterGroup::ZeroOutputBufferIfNeeded() {
  const int num_output_frames = output_config_.output_frames_per_write;
  if (output_frames_zeroed_ < num_output_frames) {
    float* buffer = GetOutputBuffer();
    std::fill_n(buffer, num_output_frames * GetOutputChannelCount(), 0);
    output_frames_zeroed_ = num_output_frames;
  }
}

void FilterGroup::ResizeBuffers() {
  int frames =
      std::max(input_frames_per_write_, output_config_.output_frames_per_write);
  int channels = std::max(num_channels_, GetOutputChannelCount());
  interleaved_.assign(frames * channels, 0.0f);
  output_frames_zeroed_ = 0;
}

void FilterGroup::SetPostProcessorConfig(const std::string& name,
                                         const std::string& config) {
  post_processing_pipeline_->SetPostProcessorConfig(name, config);
  post_processing_configs_.insert_or_assign(name, config);
  for (MixerInput* input : active_inputs_) {
    input->SetPostProcessorConfig(name, config);
  }
}

bool FilterGroup::IsRinging() const {
  return post_processing_pipeline_->IsRinging();
}

void FilterGroup::PrintTopology() const {
  std::string filter_groups;
  for (const auto& mixed_input : mixed_inputs_) {
    mixed_input.group->PrintTopology();
    filter_groups += "[GROUP]" + mixed_input.group->name() + ", ";
  }

  std::string input_groups;
  for (const std::string& stream_type : stream_types_) {
    input_groups += "[STREAM]" + stream_type + ", ";
  }

  // Trim trailing comma.
  if (!filter_groups.empty()) {
    filter_groups.resize(filter_groups.size() - 2);
  }
  if (!input_groups.empty()) {
    input_groups.resize(input_groups.size() - 2);
  }

  std::string all_inputs;
  if (filter_groups.empty()) {
    all_inputs = input_groups;
  } else if (input_groups.empty()) {
    all_inputs = filter_groups;
  } else {
    all_inputs = input_groups + " + " + filter_groups;
  }
  LOG(INFO) << all_inputs << ": " << num_channels_ << "ch@"
            << input_samples_per_second_ << "hz -> [GROUP]" << name_ << " -> "
            << GetOutputChannelCount() << "ch@"
            << output_config_.output_sample_rate << "hz";
}

}  // namespace media
}  // namespace chromecast
