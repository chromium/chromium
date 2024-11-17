// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer/mixer_input.h"

#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "chromecast/media/audio/audio_fader.h"
#include "chromecast/media/audio/audio_log.h"
#include "chromecast/media/audio/interleaved_channel_mixer.h"
#include "chromecast/media/cma/backend/mixer/audio_output_redirector_input.h"
#include "chromecast/media/cma/backend/mixer/channel_layout.h"
#include "chromecast/media/cma/backend/mixer/filter_group.h"
#include "chromecast/media/cma/backend/mixer/post_processing_pipeline.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/multi_channel_resampler.h"

namespace chromecast {
namespace media {

namespace {

const int64_t kMicrosecondsPerSecond = 1000 * 1000;
const int kDefaultSlewTimeMs = 50;
const int kDefaultFillBufferFrames = 2048;
constexpr int kMaxChannels = 32;

int RoundUpMultiple(int value, int multiple) {
  return multiple * ((value + (multiple - 1)) / multiple);
}

}  // namespace

MixerInput::MixerInput(Source* source, FilterGroup* filter_group)
    : source_(source),
      num_channels_(source->num_channels()),
      channel_layout_(source->channel_layout()),
      input_samples_per_second_(source->sample_rate()),
      output_samples_per_second_(filter_group->input_samples_per_second()),
      primary_(source->primary()),
      device_id_(source->device_id()),
      content_type_(source->content_type()),
      slew_volume_(kDefaultSlewTimeMs, true) {
  DCHECK(source_);
  DCHECK(filter_group);
  DCHECK_GT(num_channels_, 0);
  DCHECK_GT(input_samples_per_second_, 0);
  DETACH_FROM_SEQUENCE(sequence_checker_);

  fill_buffer_ =
      ::media::AudioBus::Create(num_channels_, kDefaultFillBufferFrames);
  fill_buffer_->Zero();
  interleaved_.assign(kDefaultFillBufferFrames * num_channels_, 0.0f);

  source_read_size_ = filter_group->input_frames_per_write();
  if (output_samples_per_second_ == 0) {
    // Mixer is not running. OnError() will be called shortly.
    return;
  }
  if (source_->require_clock_rate_simulation() ||
      output_samples_per_second_ != input_samples_per_second_) {
    if (source_->require_clock_rate_simulation()) {
      // Minimize latency.
      source_read_size_ = ::media::SincResampler::kSmallRequestSize;
    } else {
      // Round up to nearest multiple of SincResampler::kMaxKernelSize. The read
      // size must be > kMaxKernelSize, so we round up to at least 2 *
      // kMaxKernelSize.
      source_read_size_ =
          RoundUpMultiple(std::max(source_->desired_read_size(),
                                   ::media::SincResampler::kMaxKernelSize + 1),
                          ::media::SincResampler::kMaxKernelSize);
    }
    resample_ratio_ = static_cast<double>(input_samples_per_second_) /
                      output_samples_per_second_;
    resampler_ = std::make_unique<::media::MultiChannelResampler>(
        num_channels_, resample_ratio_, source_read_size_,
        base::BindRepeating(&MixerInput::ResamplerReadCallback,
                            base::Unretained(this)));
    resampler_->PrimeWithSilence();
  }

  slew_volume_.SetSampleRate(output_samples_per_second_);

  SetFilterGroupInternal(filter_group);
  // Don't add to the filter group yet, since this may not be the correct
  // sequence. It will be added in Initialize().
}

MixerInput::~MixerInput() {
  source_->FinalizeAudioPlayback();
}

void MixerInput::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(filter_group_);
  MediaPipelineBackend::AudioDecoder::RenderingDelay initial_rendering_delay =
      filter_group_->GetRenderingDelayToOutput();
  initial_rendering_delay.delay_microseconds +=
      prerender_delay_seconds_ * base::Time::kMicrosecondsPerSecond;

  if (resampler_) {
    double resampler_queued_frames = resampler_->BufferedFrames();
    initial_rendering_delay.delay_microseconds +=
        static_cast<int64_t>(resampler_queued_frames * kMicrosecondsPerSecond /
                             input_samples_per_second_);
  }

  source_->InitializeAudioPlayback(source_read_size_, initial_rendering_delay);
  filter_group_->AddInput(this);
}

void MixerInput::Destroy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetFilterGroup(nullptr);
}

void MixerInput::SetFilterGroup(FilterGroup* filter_group) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (SetFilterGroupInternal(filter_group)) {
    filter_group->AddInput(this);
  }
}

bool MixerInput::SetFilterGroupInternal(FilterGroup* filter_group) {
  if (output_samples_per_second_ == 0) {
    LOG(ERROR) << "Attempt to set filter group when output sample rate is 0";
    return false;
  }
  if (filter_group == filter_group_ &&
      (!filter_group || filter_group_tag_ == filter_group->tag())) {
    return false;
  }
  if (filter_group_) {
    filter_group_->RemoveInput(this);
  }
  if (filter_group) {
    filter_group_tag_ = filter_group->tag();

    prerender_pipeline_ = filter_group->CreatePrerenderPipeline(num_channels_);
    playout_channel_ = source_->playout_channel();

    AudioPostProcessor2::Config config;
    config.output_sample_rate = output_samples_per_second_;
    config.system_output_sample_rate =
        filter_group->system_output_sample_rate();
    config.output_frames_per_write = filter_group->input_frames_per_write();
    CHECK(prerender_pipeline_->SetOutputConfig(config));
    prerender_pipeline_->SetContentType(content_type_);
    prerender_pipeline_->UpdatePlayoutChannel(playout_channel_);
    DCHECK_EQ(prerender_pipeline_->GetInputSampleRate(),
              output_samples_per_second_);
    DCHECK_EQ(prerender_pipeline_->NumOutputChannels(), num_channels_);
    prerender_delay_seconds_ = prerender_pipeline_->GetDelaySeconds();

    CreateChannelMixer(playout_channel_, filter_group);
  }
  filter_group_ = filter_group;
  return (filter_group != nullptr);
}

void MixerInput::SetPostProcessorConfig(const std::string& name,
                                        const std::string& config) {
  prerender_pipeline_->SetPostProcessorConfig(name, config);
}

void MixerInput::CreateChannelMixer(int playout_channel,
                                    FilterGroup* filter_group) {
  int effective_channels = num_channels_;
  ::media::ChannelLayout channel_layout = channel_layout_;
  if (playout_channel != kChannelAll && playout_channel < num_channels_) {
    effective_channels = 1;
    channel_layout = ::media::CHANNEL_LAYOUT_MONO;
  }
  channel_mixer_ = std::make_unique<InterleavedChannelMixer>(
      channel_layout, effective_channels,
      mixer::GuessChannelLayout(filter_group->num_channels()),
      filter_group->num_channels(), filter_group->input_frames_per_write());
}

void MixerInput::AddAudioOutputRedirector(
    AudioOutputRedirectorInput* redirector) {
  AUDIO_LOG(INFO) << "Add redirector to " << device_id_ << "(" << source_
                  << ")";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(redirector);
  audio_output_redirectors_.insert(
      std::upper_bound(
          audio_output_redirectors_.begin(), audio_output_redirectors_.end(),
          redirector,
          [](AudioOutputRedirectorInput* a, AudioOutputRedirectorInput* b) {
            return (a->Order() < b->Order());
          }),
      redirector);
}

void MixerInput::RemoveAudioOutputRedirector(
    AudioOutputRedirectorInput* redirector) {
  AUDIO_LOG(INFO) << "Remove redirector from " << device_id_ << "(" << source_
                  << ")";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(redirector);
  audio_output_redirectors_.erase(
      std::remove(audio_output_redirectors_.begin(),
                  audio_output_redirectors_.end(), redirector),
      audio_output_redirectors_.end());
}

bool MixerInput::Render(
    int num_output_frames,
    MediaPipelineBackend::AudioDecoder::RenderingDelay rendering_delay) {
  if (num_output_frames > fill_buffer_->frames()) {
    fill_buffer_ = ::media::AudioBus::Create(num_channels_, num_output_frames);
    interleaved_.assign(num_output_frames * num_channels_, 0.0f);
  }

  if (incomplete_previous_fill_) {
    slew_volume_.Interrupted();
  }

  rendering_delay.delay_microseconds +=
      prerender_delay_seconds_ * base::Time::kMicrosecondsPerSecond;
  int filled =
      FillAudioData(num_output_frames, rendering_delay, fill_buffer_.get());
  if (filled == 0) {
    if (!prerender_pipeline_->IsRinging()) {
      render_output_ = nullptr;
      return false;
    }
  }

  fill_buffer_->ZeroFramesPartial(filled, num_output_frames - filled);

  fill_buffer_->ToInterleaved<::media::FloatSampleTypeTraitsNoClip<float>>(
      num_output_frames, interleaved_.data());
  slew_volume_.ProcessFMUL(false /* repeat_transition */, interleaved_.data(),
                           num_output_frames, num_channels_,
                           interleaved_.data());

  const int playout_channel = source_->playout_channel();
  if (playout_channel != playout_channel_) {
    prerender_pipeline_->UpdatePlayoutChannel(playout_channel);
    CreateChannelMixer(playout_channel, filter_group_);
    playout_channel_ = playout_channel;
  }

  const bool is_silence = (filled == 0);
  prerender_pipeline_->ProcessFrames(interleaved_.data(), num_output_frames,
                                     InstantaneousVolume(), TargetVolume(),
                                     is_silence);
  prerender_delay_seconds_ = prerender_pipeline_->GetDelaySeconds();

  RenderInterleaved(num_output_frames);
  return true;
}

void MixerInput::RenderInterleaved(int num_output_frames) {
  // TODO(kmackay): If we ever support channel selections other than L and R,
  // we should remix channels to a format that includes the selected channel
  // and then do channel selection. Currently if the input is mono we don't
  // bother doing channel selection since the result would be the same as
  // doing nothing anyway.
  float* data = prerender_pipeline_->GetOutputBuffer();

  if (playout_channel_ != kChannelAll && playout_channel_ < num_channels_) {
    // Keep only the samples from the selected channel.
    float* dest = interleaved_.data();
    for (int f = 0; f < num_output_frames; ++f) {
      dest[f] = data[f * num_channels_ + playout_channel_];
    }
    data = dest;
  }

  render_output_ = channel_mixer_->Transform(data, num_output_frames);
}

float* MixerInput::RenderedAudioBuffer() {
  DCHECK(render_output_);
  return render_output_;
}

int MixerInput::FillAudioData(int num_frames,
                              RenderingDelay rendering_delay,
                              ::media::AudioBus* dest) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(dest);
  DCHECK_GE(dest->frames(), num_frames);

  RenderingDelay redirected_delay = rendering_delay;
  if (!audio_output_redirectors_.empty()) {
    redirected_delay.delay_microseconds +=
        audio_output_redirectors_[0]->GetDelayMicroseconds();
  }
  int filled = FillBuffer(num_frames, redirected_delay, dest);
  incomplete_previous_fill_ = (filled != num_frames);

  bool redirected = false;
  for (auto* redirector : audio_output_redirectors_) {
    redirector->Redirect(dest, filled, rendering_delay, redirected);
    redirected = true;
  }

  CHECK_LE(num_channels_, kMaxChannels);
  float* channels[kMaxChannels];
  for (int c = 0; c < num_channels_; ++c) {
    channels[c] = dest->channel(c);
  }
  if (first_buffer_ && redirected) {
    // If the first buffer is redirected, don't provide any data to the mixer
    // (we want to avoid a 'blip' of sound from the first buffer if it is being
    // redirected).
    filled = 0;
  } else if (previous_ended_in_silence_) {
    if (redirected) {
      // Previous buffer ended in silence, and the current buffer was redirected
      // by the output chain, so maintain silence.
      filled = 0;
    } else {
      // Smoothly fade in from previous silence.
      AudioFader::FadeInHelper(channels, num_channels_, filled, filled, filled);
    }
  } else if (redirected) {
    // Smoothly fade out to silence, since output is now being redirected.
    AudioFader::FadeOutHelper(channels, num_channels_, filled, filled, filled);
  }
  previous_ended_in_silence_ = redirected;
  first_buffer_ = false;

  return filled;
}

int MixerInput::FillBuffer(int num_frames,
                           RenderingDelay rendering_delay,
                           ::media::AudioBus* dest) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(dest);
  DCHECK_EQ(num_channels_, dest->channels());
  DCHECK_GE(dest->frames(), num_frames);

  if (resampler_) {
    mixer_rendering_delay_ = rendering_delay;
    // resampler_->BufferedFrames() gives incorrect values in the read callback,
    // so track the number of buffered frames ourselves.
    // Based on testing, the buffered frames reported by SincResampler does not
    // include the delay incurred by the filter kernel, so add it explicitly.
    resampler_buffered_frames_ =
        resampler_->BufferedFrames() + resampler_->KernelSize() / 2;
    filled_for_resampler_ = 0;
    tried_to_fill_resampler_ = false;
    resampler_->Resample(num_frames, dest);
    // If the source is not providing any audio anymore, we want to stop filling
    // frames so we can reduce processing overhead. However, since the resampler
    // fill size doesn't necessarily match the mixer's request size at all, we
    // need to be careful. The resampler could have a lot of data buffered
    // internally, so we only count cases where the resampler needed more data
    // from the source but none was available. Then, to make sure all data is
    // flushed out of the resampler, we require that to happen twice before we
    // stop filling audio.
    if (tried_to_fill_resampler_) {
      if (filled_for_resampler_ == 0) {
        resampled_silence_count_ = std::min(resampled_silence_count_ + 1, 2);
      } else {
        resampled_silence_count_ = 0;
      }
    }
    if (resampled_silence_count_ > 1) {
      return 0;
    }
    return num_frames;
  } else {
    return source_->FillAudioPlaybackFrames(num_frames, rendering_delay, dest);
  }
}

void MixerInput::ResamplerReadCallback(int frame_delay,
                                       ::media::AudioBus* output) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RenderingDelay delay = mixer_rendering_delay_;
  int64_t resampler_delay =
      std::round(resampler_buffered_frames_ * kMicrosecondsPerSecond /
                 input_samples_per_second_);
  delay.delay_microseconds += resampler_delay;

  const int needed_frames = output->frames();
  tried_to_fill_resampler_ = true;
  int filled = source_->FillAudioPlaybackFrames(needed_frames, delay, output);
  filled_for_resampler_ += filled;
  if (filled < needed_frames) {
    output->ZeroFramesPartial(filled, needed_frames - filled);
  }
  resampler_buffered_frames_ += output->frames();
}

void MixerInput::SignalError(Source::MixerError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (filter_group_) {
    filter_group_->RemoveInput(this);
    filter_group_ = nullptr;
  }
  source_->OnAudioPlaybackError(error);
}

void MixerInput::VolumeScaleAccumulate(const float* src,
                                       int frames,
                                       float* dest,
                                       int channel_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const bool same_volume_transition = (channel_index != 0);
  slew_volume_.ProcessFMAC(same_volume_transition, src, frames, 1, dest);
}

void MixerInput::SetVolumeMultiplier(float multiplier) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  float old_target_volume = TargetVolume();
  stream_volume_multiplier_ = std::max(0.0f, multiplier);
  float target_volume = TargetVolume();
  AUDIO_LOG(INFO) << device_id_ << "(" << source_
                  << "): stream volume = " << stream_volume_multiplier_
                  << ", effective multiplier = " << target_volume;
  if (target_volume != old_target_volume) {
    slew_volume_.SetMaxSlewTimeMs(kDefaultSlewTimeMs);
    slew_volume_.SetVolume(target_volume);
  }
}

void MixerInput::SetContentTypeVolume(float volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(content_type_ != AudioContentType::kOther);

  float old_target_volume = TargetVolume();
  type_volume_multiplier_ = volume;
  float target_volume = TargetVolume();
  AUDIO_LOG(INFO) << device_id_ << "(" << source_
                  << "): type volume = " << type_volume_multiplier_
                  << ", effective multiplier = " << target_volume;
  if (target_volume != old_target_volume) {
    slew_volume_.SetMaxSlewTimeMs(kDefaultSlewTimeMs);
    slew_volume_.SetVolume(target_volume);
  }
}

void MixerInput::SetVolumeLimits(float volume_min, float volume_max) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  float old_target_volume = TargetVolume();
  volume_min_ = volume_min;
  volume_max_ = volume_max;
  float target_volume = TargetVolume();
  AUDIO_LOG(INFO) << device_id_ << "(" << source_ << "): set volume limits to ["
                  << volume_min_ << ", " << volume_max_ << "]";
  if (target_volume != old_target_volume) {
    slew_volume_.SetMaxSlewTimeMs(kDefaultSlewTimeMs);
    slew_volume_.SetVolume(target_volume);
  }
}

void MixerInput::SetOutputLimit(float limit, int fade_ms) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  float old_target_volume = TargetVolume();
  output_volume_limit_ = limit;
  float target_volume = TargetVolume();
  AUDIO_LOG(INFO) << device_id_ << "(" << source_
                  << "): output limit = " << output_volume_limit_
                  << ", effective multiplier = " << target_volume;
  if (fade_ms < 0) {
    fade_ms = kDefaultSlewTimeMs;
  } else {
    AUDIO_LOG(INFO) << "Fade over " << fade_ms << " ms";
  }
  if (target_volume != old_target_volume) {
    slew_volume_.SetMaxSlewTimeMs(fade_ms);
    slew_volume_.SetVolume(target_volume);
  }
}

void MixerInput::SetMuted(bool muted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(content_type_ != AudioContentType::kOther);

  float old_target_volume = TargetVolume();
  mute_volume_multiplier_ = muted ? 0.0f : 1.0f;
  float target_volume = TargetVolume();
  AUDIO_LOG(INFO) << device_id_ << "(" << source_
                  << "): mute volume = " << mute_volume_multiplier_
                  << ", effective multiplier = " << target_volume;
  if (target_volume != old_target_volume) {
    slew_volume_.SetMaxSlewTimeMs(kDefaultSlewTimeMs);
    slew_volume_.SetVolume(target_volume);
  }
}

float MixerInput::TargetVolume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  float output_volume = stream_volume_multiplier_ * type_volume_multiplier_;
  float clamped_volume = std::clamp(output_volume, volume_min_, volume_max_);
  float limited_volume = std::min(clamped_volume, output_volume_limit_);
  float muted_volume = limited_volume * mute_volume_multiplier_;
  // Volume is clamped after all gains have been multiplied, to avoid clipping.
  // TODO(kmackay): Consider removing this clamp and use a postprocessor filter
  // to avoid clipping instead.
  return std::clamp(muted_volume, 0.0f, 1.0f);
}

float MixerInput::InstantaneousVolume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return slew_volume_.LastBufferMaxMultiplier();
}

void MixerInput::SetSimulatedClockRate(double new_clock_rate) {
  if (new_clock_rate == simulated_clock_rate_ || !resampler_) {
    return;
  }
  simulated_clock_rate_ = new_clock_rate;
  resampler_->SetRatio(resample_ratio_ * simulated_clock_rate_);
}

}  // namespace media
}  // namespace chromecast
