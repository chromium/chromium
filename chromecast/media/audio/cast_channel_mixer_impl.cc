// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/api/cast_channel_mixer.h"

#include "chromecast/media/cma/base/decoder_config_adapter.h"
#include "media/base/audio_bus.h"
#include "media/base/channel_layout.h"
#include "media/base/channel_mixer.h"

namespace chromecast {
namespace media {
namespace {

void SetChannelData(::media::AudioBus* bus, float* data, int num_frames) {
  bus->set_frames(num_frames);
  for (int channel = 0; channel < bus->channels(); channel++) {
    bus->SetChannelData(channel, data + num_frames * channel);
  }
}

// A wrapper of ::media::ChannelMixer.
class CastChannelMixerImpl : public CastChannelMixer {
 public:
  CastChannelMixerImpl(ChannelLayout input, ChannelLayout output);
  ~CastChannelMixerImpl() override;

  const float* Transform(const float* input, int num_frames) override;

 private:
  std::unique_ptr<::media::ChannelMixer> channel_mixer_;

  std::unique_ptr<::media::AudioBus> input_bus_;
  std::unique_ptr<::media::AudioBus> output_bus_;

  std::vector<float> output_buffer_;
};

CastChannelMixerImpl::CastChannelMixerImpl(ChannelLayout input,
                                           ChannelLayout output) {
  ::media::ChannelLayout input_layout =
      DecoderConfigAdapter::ToMediaChannelLayout(input);
  ::media::ChannelLayout output_layout =
      DecoderConfigAdapter::ToMediaChannelLayout(output);
  channel_mixer_ = std::make_unique<::media::ChannelMixer>(
      input_layout, ::media::ChannelLayoutToChannelCount(input_layout),
      output_layout, ::media::ChannelLayoutToChannelCount(output_layout));
  input_bus_ = ::media::AudioBus::CreateWrapper(
      ::media::ChannelLayoutToChannelCount(input_layout));
  output_bus_ = ::media::AudioBus::CreateWrapper(
      ::media::ChannelLayoutToChannelCount(output_layout));
}

CastChannelMixerImpl::~CastChannelMixerImpl() = default;

const float* CastChannelMixerImpl::Transform(const float* input,
                                             int num_frames) {
  SetChannelData(input_bus_.get(), const_cast<float*>(input), num_frames);

  if (output_buffer_.size() <
      static_cast<size_t>(num_frames * output_bus_->channels())) {
    output_buffer_.resize(num_frames * output_bus_->channels());
  }
  SetChannelData(output_bus_.get(), output_buffer_.data(), num_frames);

  channel_mixer_->Transform(input_bus_.get(), output_bus_.get());
  return output_buffer_.data();
}

}  // namespace

std::unique_ptr<CastChannelMixer> CastChannelMixer::Create(
    ChannelLayout input,
    ChannelLayout output) {
  return std::make_unique<CastChannelMixerImpl>(input, output);
}

}  // namespace media
}  // namespace chromecast