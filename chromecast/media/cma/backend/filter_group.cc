// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/filter_group.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromecast/media/cma/backend/mixer_input.h"
#include "chromecast/media/cma/backend/post_processing_pipeline.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_sample_types.h"
#include "media/base/vector_math.h"

namespace chromecast {
namespace media {

FilterGroup::FilterGroup(int num_channels,
                         GroupType type,
                         const std::string& name,
                         std::unique_ptr<PostProcessingPipeline> pipeline,
                         const base::flat_set<std::string>& device_ids,
                         const std::vector<FilterGroup*>& mixed_inputs)
    : num_channels_(num_channels),
      type_(type),
      mix_to_mono_(false),
      playout_channel_(kChannelAll),
      name_(name),
      device_ids_(device_ids),
      mixed_inputs_(mixed_inputs),
      output_samples_per_second_(0),
      frames_zeroed_(0),
      last_volume_(0.0),
      delay_frames_(0),
      content_type_(AudioContentType::kMedia),
      post_processing_pipeline_(std::move(pipeline)) {
  for (auto* const m : mixed_inputs) {
    DCHECK_EQ(m->GetOutputChannelCount(), num_channels);
  }
}

FilterGroup::~FilterGroup() = default;

void FilterGroup::Initialize(int output_samples_per_second) {
  output_samples_per_second_ = output_samples_per_second;
  CHECK(post_processing_pipeline_->SetSampleRate(output_samples_per_second));
  post_processing_pipeline_->SetContentType(content_type_);
  active_inputs_.clear();
}

bool FilterGroup::CanProcessInput(const std::string& input_device_id) {
  return !(device_ids_.find(input_device_id) == device_ids_.end());
}

void FilterGroup::AddInput(MixerInput* input) {
  active_inputs_.insert(input);
  if (mixed_) {
    AddTempBuffer(input->num_channels(), mixed_->frames());
  }
}

void FilterGroup::RemoveInput(MixerInput* input) {
  active_inputs_.erase(input);
}

float FilterGroup::MixAndFilter(
    int num_frames,
    MediaPipelineBackend::AudioDecoder::RenderingDelay rendering_delay) {
  DCHECK_NE(output_samples_per_second_, 0);

  bool resize_needed = ResizeBuffersIfNecessary(num_frames);

  float volume = 0.0f;
  AudioContentType content_type = static_cast<AudioContentType>(-1);

  rendering_delay.delay_microseconds += GetRenderingDelayMicroseconds();
  rendering_delay_to_output_ = rendering_delay;

  // Recursively mix inputs.
  for (auto* filter_group : mixed_inputs_) {
    volume = std::max(volume,
                      filter_group->MixAndFilter(num_frames, rendering_delay));
    content_type = std::max(content_type, filter_group->content_type());
  }

  // |volume| can only be 0 if no |mixed_inputs_| have data.
  // This is true because FilterGroup can only return 0 if:
  // a) It has no data and its PostProcessorPipeline is not ringing.
  //    (early return, below) or
  // b) The output volume is 0 and has NEVER been non-zero,
  //    since FilterGroup will use last_volume_ if volume is 0.
  //    In this case, there was never any data in the pipeline.
  // However, we still need to ensure that output buffers are initialized &
  // large enough to hold |num_frames|, so we cannot use this shortcut if
  // |resize_needed|.
  if (active_inputs_.empty() && volume == 0.0f &&
      !post_processing_pipeline_->IsRinging() && !resize_needed) {
    if (frames_zeroed_ < num_frames) {
      // Ensure OutputBuffer() is zeros.
      // TODO(bshaya): Determine if this is necessary - if RingingTime is
      //               calculated correctly, then we could skip the fill_n.
      std::fill_n(GetOutputBuffer(), num_frames * GetOutputChannelCount(), 0);
      frames_zeroed_ = num_frames;
    }
    return 0.0f;  // Output will be silence, no need to mix.
  }

  frames_zeroed_ = 0;

  // Mix InputQueues
  mixed_->ZeroFramesPartial(0, num_frames);
  for (MixerInput* input : active_inputs_) {
    DCHECK_LT(input->num_channels(), static_cast<int>(temp_buffers_.size()));
    DCHECK(temp_buffers_[input->num_channels()]);
    ::media::AudioBus* temp = temp_buffers_[input->num_channels()].get();
    int filled = input->FillAudioData(num_frames, rendering_delay, temp);
    if (filled > 0) {
      int in_c = 0;
      for (int out_c = 0; out_c < num_channels_; ++out_c) {
        input->VolumeScaleAccumulate(temp->channel(in_c), filled,
                                     mixed_->channel(out_c));
        ++in_c;
        if (in_c >= input->num_channels()) {
          in_c = 0;
        }
      }

      volume = std::max(volume, input->InstantaneousVolume());
      content_type = std::max(content_type, input->content_type());
    }
  }

  mixed_->ToInterleaved<::media::FloatSampleTypeTraits<float>>(
      num_frames, interleaved_.get());

  // Mix FilterGroups
  for (FilterGroup* group : mixed_inputs_) {
    if (group->last_volume() > 0.0f) {
      ::media::vector_math::FMAC(group->interleaved_.get(), 1.0f,
                                 num_frames * num_channels_,
                                 interleaved_.get());
    }
  }

  // Copy the active channel to all channels. Only used in the "linearize"
  // instance.
  if (playout_channel_ != kChannelAll && type_ == GroupType::kLinearize) {
    DCHECK_GE(playout_channel_, 0);
    DCHECK_LT(playout_channel_, num_channels_);

    for (int frame = 0; frame < num_frames; ++frame) {
      float s = interleaved_.get()[frame * num_channels_ + playout_channel_];
      for (int c = 0; c < num_channels_; ++c)
        interleaved_.get()[frame * num_channels_ + c] = s;
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

  delay_frames_ = post_processing_pipeline_->ProcessFrames(
      interleaved_.get(), num_frames, last_volume_, is_silence);

  // Mono mixing if needed. Only used in the "Mix" instance.
  if (mix_to_mono_ && type_ == GroupType::kFinalMix) {
    for (int frame = 0; frame < num_frames; ++frame) {
      float sum = 0;
      for (int c = 0; c < num_channels_; ++c)
        sum += interleaved_.get()[frame * num_channels_ + c];
      for (int c = 0; c < num_channels_; ++c)
        interleaved_.get()[frame * num_channels_ + c] = sum / num_channels_;
    }
  }

  return last_volume_;
}

float* FilterGroup::GetOutputBuffer() {
  return post_processing_pipeline_->GetOutputBuffer();
}

int64_t FilterGroup::GetRenderingDelayMicroseconds() {
  if (output_samples_per_second_ == 0) {
    return 0;
  }
  return delay_frames_ * base::Time::kMicrosecondsPerSecond /
         output_samples_per_second_;
}

MediaPipelineBackend::AudioDecoder::RenderingDelay
FilterGroup::GetRenderingDelayToOutput() {
  return rendering_delay_to_output_;
}

int FilterGroup::GetOutputChannelCount() {
  return post_processing_pipeline_->NumOutputChannels();
}

bool FilterGroup::ResizeBuffersIfNecessary(int num_frames) {
  if (mixed_ && mixed_->frames() >= num_frames) {
    return false;
  }
  mixed_ = ::media::AudioBus::Create(num_channels_, num_frames);
  temp_buffers_.clear();
  for (MixerInput* input : active_inputs_) {
    AddTempBuffer(input->num_channels(), num_frames);
  }
  interleaved_.reset(static_cast<float*>(
      base::AlignedAlloc(num_frames * num_channels_ * sizeof(float),
                         ::media::AudioBus::kChannelAlignment)));
  return true;
}

void FilterGroup::AddTempBuffer(int num_channels, int num_frames) {
  if (static_cast<int>(temp_buffers_.size()) <= num_channels) {
    temp_buffers_.resize(num_channels + 1);
  }
  if (!temp_buffers_[num_channels]) {
    temp_buffers_[num_channels] =
        ::media::AudioBus::Create(num_channels, num_frames);
  }
}

void FilterGroup::SetPostProcessorConfig(const std::string& name,
                                         const std::string& config) {
  post_processing_pipeline_->SetPostProcessorConfig(name, config);
}

void FilterGroup::SetMixToMono(bool mix_to_mono) {
  mix_to_mono_ = (num_channels_ != 1 && mix_to_mono);
}

void FilterGroup::UpdatePlayoutChannel(int playout_channel) {
  if (playout_channel >= num_channels_) {
    LOG(ERROR) << "only " << num_channels_ << " present, wanted channel #"
               << playout_channel;
    return;
  }
  playout_channel_ = playout_channel;
  post_processing_pipeline_->UpdatePlayoutChannel(playout_channel_);
}

}  // namespace media
}  // namespace chromecast
