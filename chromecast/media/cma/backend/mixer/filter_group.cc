// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer/filter_group.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromecast/media/cma/backend/interleaved_channel_mixer.h"
#include "chromecast/media/cma/backend/mixer/mixer_input.h"
#include "chromecast/media/cma/backend/mixer/post_processing_pipeline.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_sample_types.h"
#include "media/base/vector_math.h"

namespace chromecast {
namespace media {

FilterGroup::GroupInput::GroupInput(
    FilterGroup* group,
    std::unique_ptr<InterleavedChannelMixer> channel_mixer)
    : group(group), channel_mixer(std::move(channel_mixer)) {}

FilterGroup::GroupInput::GroupInput(GroupInput&& other) = default;
FilterGroup::GroupInput::~GroupInput() = default;

FilterGroup::FilterGroup(int num_channels,
                         const std::string& name,
                         std::unique_ptr<PostProcessingPipeline> pipeline)
    : num_channels_(num_channels),
      name_(name),
      post_processing_pipeline_(std::move(pipeline)) {}

FilterGroup::~FilterGroup() = default;

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
        ::media::GuessChannelLayout(input.group->GetOutputChannelCount()),
        ::media::GuessChannelLayout(num_channels_), input_frames_per_write_);
  }
  post_processing_pipeline_->SetContentType(content_type_);
  active_inputs_.clear();
  ResizeBuffers();

  // Run a buffer of 0's to initialize rendering delay.
  std::fill_n(interleaved_.data(), interleaved_.size(), 0.0f);
  delay_seconds_ = post_processing_pipeline_->ProcessFrames(
      interleaved_.data(), input_frames_per_write_, last_volume_,
      true /* is_silence */);
}

void FilterGroup::AddInput(MixerInput* input) {
  active_inputs_.insert(input);
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
  AudioContentType content_type = static_cast<AudioContentType>(-1);

  rendering_delay.delay_microseconds += GetRenderingDelayMicroseconds();
  rendering_delay_to_output_ = rendering_delay;

  // Recursively mix inputs.
  for (const auto& filter_group : mixed_inputs_) {
    volume = std::max(volume, filter_group.group->MixAndFilter(
                                  input_frames_per_write_, rendering_delay));
    content_type = std::max(content_type, filter_group.group->content_type());
  }

  // |volume| can only be 0 if no |mixed_inputs_| have data.
  // This is true because FilterGroup can only return 0 if:
  // a) It has no data and its PostProcessorPipeline is not ringing.
  //    (early return, below) or
  // b) The output volume is 0 and has NEVER been non-zero,
  //    since FilterGroup will use last_volume_ if volume is 0.
  //    In this case, there was never any data in the pipeline.
  if (active_inputs_.empty() && volume == 0.0f &&
      !post_processing_pipeline_->IsRinging()) {
    if (frames_zeroed_ < num_output_frames) {
      std::fill_n(GetOutputBuffer(),
                  num_output_frames * GetOutputChannelCount(), 0);
      frames_zeroed_ = num_output_frames;
    }
    return 0.0f;  // Output will be silence, no need to mix.
  }

  frames_zeroed_ = 0;

  // Mix InputQueues
  mixed_->ZeroFramesPartial(0, input_frames_per_write_);
  for (MixerInput* input : active_inputs_) {
    ::media::AudioBus* temp = temp_buffer_.get();
    int filled =
        input->FillAudioData(input_frames_per_write_, rendering_delay, temp);
    if (filled > 0) {
      for (int c = 0; c < num_channels_; ++c) {
        input->VolumeScaleAccumulate(temp->channel(c), filled,
                                     mixed_->channel(c));
      }

      volume = std::max(volume, input->InstantaneousVolume());
      content_type = std::max(content_type, input->content_type());
    }
  }

  mixed_->ToInterleaved<::media::FloatSampleTypeTraitsNoClip<float>>(
      input_frames_per_write_, interleaved_.data());

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

  // Allow paused streams to "ring out" at the last valid volume.
  // If the stream volume is actually 0, this doesn't matter, since the
  // data is 0's anyway.
  bool is_silence = (volume == 0.0f);
  if (!is_silence) {
    last_volume_ = volume;
    DCHECK_NE(-1, static_cast<int>(content_type))
        << "Got frames without content type.";
    if (content_type != content_type_) {
      content_type_ = content_type;
      post_processing_pipeline_->SetContentType(content_type_);
    }
  }

  delay_seconds_ = post_processing_pipeline_->ProcessFrames(
      interleaved_.data(), input_frames_per_write_, last_volume_, is_silence);
  return last_volume_;
}

float* FilterGroup::GetOutputBuffer() {
  return post_processing_pipeline_->GetOutputBuffer();
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

void FilterGroup::ResizeBuffers() {
  mixed_ = ::media::AudioBus::Create(num_channels_, input_frames_per_write_);
  mixed_->Zero();
  temp_buffer_ =
      ::media::AudioBus::Create(num_channels_, input_frames_per_write_);
  temp_buffer_->Zero();
  interleaved_.assign(input_frames_per_write_ * num_channels_, 0.0f);
}

void FilterGroup::SetPostProcessorConfig(const std::string& name,
                                         const std::string& config) {
  post_processing_pipeline_->SetPostProcessorConfig(name, config);
}

void FilterGroup::UpdatePlayoutChannel(int playout_channel) {
  if (playout_channel >= num_channels_) {
    LOG(ERROR) << "only " << num_channels_ << " present, wanted channel #"
               << playout_channel;
    return;
  }
  post_processing_pipeline_->UpdatePlayoutChannel(playout_channel);
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
