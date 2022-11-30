// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/mojom/decoder_config_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<chromecast::media::mojom::AudioConfigDataView,
                  chromecast::media::AudioConfig>::
    Read(chromecast::media::mojom::AudioConfigDataView input,
         chromecast::media::AudioConfig* output) {
  chromecast::media::StreamId id;
  if (!input.ReadId(&id)) {
    return false;
  }

  chromecast::media::AudioCodec codec;
  if (!input.ReadCodec(&codec)) {
    return false;
  }

  chromecast::media::ChannelLayout channel_layout;
  if (!input.ReadChannelLayout(&channel_layout)) {
    return false;
  }

  chromecast::media::SampleFormat sample_format;
  if (!input.ReadSampleFormat(&sample_format)) {
    return false;
  }

  std::vector<uint8_t> extra_data;
  if (!input.ReadExtraData(&extra_data)) {
    return false;
  }

  chromecast::media::EncryptionScheme encryption_scheme;
  if (!input.ReadEncryptionScheme(&encryption_scheme)) {
    return false;
  }

  output->id = id;
  output->codec = codec;
  output->channel_layout = channel_layout;
  output->sample_format = sample_format;
  output->bytes_per_channel = input.bytes_per_channel();
  output->channel_number = input.channel_number();
  output->samples_per_second = input.samples_per_second();
  output->extra_data = std::move(extra_data);
  output->encryption_scheme = encryption_scheme;

  return true;
}

}  // namespace mojo
