// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_MOJOM_DECODER_CONFIG_MOJOM_TRAITS_H_
#define CHROMECAST_MEDIA_MOJOM_DECODER_CONFIG_MOJOM_TRAITS_H_

#include "chromecast/media/mojom/media_types.mojom-shared.h"
#include "chromecast/public/media/decoder_config.h"

namespace mojo {

template <>
struct mojo::EnumTraits<chromecast::media::mojom::AudioCodec,
                        chromecast::media::AudioCodec> {
  static chromecast::media::mojom::AudioCodec ToMojom(
      chromecast::media::AudioCodec input) {
    switch (input) {
      case (chromecast::media::AudioCodec::kAudioCodecUnknown):
        return chromecast::media::mojom::AudioCodec::kAudioCodecUnknown;
      case (chromecast::media::AudioCodec::kCodecAAC):
        return chromecast::media::mojom::AudioCodec::kCodecAAC;
      case (chromecast::media::AudioCodec::kCodecMP3):
        return chromecast::media::mojom::AudioCodec::kCodecMP3;
      case (chromecast::media::AudioCodec::kCodecPCM):
        return chromecast::media::mojom::AudioCodec::kCodecPCM;
      case (chromecast::media::AudioCodec::kCodecPCM_S16BE):
        return chromecast::media::mojom::AudioCodec::kCodecPCM_S16BE;
      case (chromecast::media::AudioCodec::kCodecVorbis):
        return chromecast::media::mojom::AudioCodec::kCodecVorbis;
      case (chromecast::media::AudioCodec::kCodecOpus):
        return chromecast::media::mojom::AudioCodec::kCodecOpus;
      case (chromecast::media::AudioCodec::kCodecEAC3):
        return chromecast::media::mojom::AudioCodec::kCodecEAC3;
      case (chromecast::media::AudioCodec::kCodecAC3):
        return chromecast::media::mojom::AudioCodec::kCodecAC3;
      case (chromecast::media::AudioCodec::kCodecDTS):
        return chromecast::media::mojom::AudioCodec::kCodecDTS;
      case (chromecast::media::AudioCodec::kCodecDTSXP2):
        return chromecast::media::mojom::AudioCodec::kCodecDTSXP2;
      case (chromecast::media::AudioCodec::kCodecDTSE):
        return chromecast::media::mojom::AudioCodec::kCodecDTSE;
      case (chromecast::media::AudioCodec::kCodecFLAC):
        return chromecast::media::mojom::AudioCodec::kCodecFLAC;
      case (chromecast::media::AudioCodec::kCodecMpegHAudio):
        return chromecast::media::mojom::AudioCodec::kCodecMpegHAudio;
    }
    DLOG(FATAL) << "Unrecognized AudioCodec";
    return chromecast::media::mojom::AudioCodec::kAudioCodecUnknown;
  }

  static chromecast::media::AudioCodec FromMojom(
      chromecast::media::mojom::AudioCodec input) {
    switch (input) {
      case (chromecast::media::mojom::AudioCodec::kAudioCodecUnknown):
        return chromecast::media::AudioCodec::kAudioCodecUnknown;
      case (chromecast::media::mojom::AudioCodec::kCodecAAC):
        return chromecast::media::AudioCodec::kCodecAAC;
      case (chromecast::media::mojom::AudioCodec::kCodecMP3):
        return chromecast::media::AudioCodec::kCodecMP3;
      case (chromecast::media::mojom::AudioCodec::kCodecPCM):
        return chromecast::media::AudioCodec::kCodecPCM;
      case (chromecast::media::mojom::AudioCodec::kCodecPCM_S16BE):
        return chromecast::media::AudioCodec::kCodecPCM_S16BE;
      case (chromecast::media::mojom::AudioCodec::kCodecVorbis):
        return chromecast::media::AudioCodec::kCodecVorbis;
      case (chromecast::media::mojom::AudioCodec::kCodecOpus):
        return chromecast::media::AudioCodec::kCodecOpus;
      case (chromecast::media::mojom::AudioCodec::kCodecEAC3):
        return chromecast::media::AudioCodec::kCodecEAC3;
      case (chromecast::media::mojom::AudioCodec::kCodecAC3):
        return chromecast::media::AudioCodec::kCodecAC3;
      case (chromecast::media::mojom::AudioCodec::kCodecDTS):
        return chromecast::media::AudioCodec::kCodecDTS;
      case (chromecast::media::mojom::AudioCodec::kCodecDTSXP2):
        return chromecast::media::AudioCodec::kCodecDTSXP2;
      case (chromecast::media::mojom::AudioCodec::kCodecDTSE):
        return chromecast::media::AudioCodec::kCodecDTSE;
      case (chromecast::media::mojom::AudioCodec::kCodecFLAC):
        return chromecast::media::AudioCodec::kCodecFLAC;
      case (chromecast::media::mojom::AudioCodec::kCodecMpegHAudio):
        return chromecast::media::AudioCodec::kCodecMpegHAudio;
    }
    NOTREACHED();
  }
};

template <>
struct mojo::EnumTraits<chromecast::media::mojom::ChannelLayout,
                        chromecast::media::ChannelLayout> {
  static chromecast::media::mojom::ChannelLayout ToMojom(
      chromecast::media::ChannelLayout input) {
    switch (input) {
      case (chromecast::media::ChannelLayout::UNSUPPORTED):
        return chromecast::media::mojom::ChannelLayout::kUnsupported;
      case (chromecast::media::ChannelLayout::MONO):
        return chromecast::media::mojom::ChannelLayout::kMono;
      case (chromecast::media::ChannelLayout::STEREO):
        return chromecast::media::mojom::ChannelLayout::kStereo;
      case (chromecast::media::ChannelLayout::SURROUND_5_1):
        return chromecast::media::mojom::ChannelLayout::kSurround_5_1;
      case (chromecast::media::ChannelLayout::BITSTREAM):
        return chromecast::media::mojom::ChannelLayout::kBitstream;
      case (chromecast::media::ChannelLayout::DISCRETE):
        return chromecast::media::mojom::ChannelLayout::kDiscrete;
    }
    DLOG(FATAL) << "Unrecognized ChannelLayout";
    return chromecast::media::mojom::ChannelLayout::kUnsupported;
  }

  static chromecast::media::ChannelLayout FromMojom(
      chromecast::media::mojom::ChannelLayout input) {
    switch (input) {
      case (chromecast::media::mojom::ChannelLayout::kUnsupported):
        return chromecast::media::ChannelLayout::UNSUPPORTED;
      case (chromecast::media::mojom::ChannelLayout::kMono):
        return chromecast::media::ChannelLayout::MONO;
      case (chromecast::media::mojom::ChannelLayout::kStereo):
        return chromecast::media::ChannelLayout::STEREO;
      case (chromecast::media::mojom::ChannelLayout::kSurround_5_1):
        return chromecast::media::ChannelLayout::SURROUND_5_1;
      case (chromecast::media::mojom::ChannelLayout::kBitstream):
        return chromecast::media::ChannelLayout::BITSTREAM;
      case (chromecast::media::mojom::ChannelLayout::kDiscrete):
        return chromecast::media::ChannelLayout::DISCRETE;
    }
    NOTREACHED();
  }
};

template <>
struct mojo::EnumTraits<chromecast::media::mojom::SampleFormat,
                        chromecast::media::SampleFormat> {
  static chromecast::media::mojom::SampleFormat ToMojom(
      chromecast::media::SampleFormat input) {
    switch (input) {
      case (chromecast::media::SampleFormat::kUnknownSampleFormat):
        return chromecast::media::mojom::SampleFormat::kUnknownSampleFormat;
      case (chromecast::media::SampleFormat::kSampleFormatU8):
        return chromecast::media::mojom::SampleFormat::kSampleFormatU8;
      case (chromecast::media::SampleFormat::kSampleFormatS16):
        return chromecast::media::mojom::SampleFormat::kSampleFormatS16;
      case (chromecast::media::SampleFormat::kSampleFormatS32):
        return chromecast::media::mojom::SampleFormat::kSampleFormatS32;
      case (chromecast::media::SampleFormat::kSampleFormatF32):
        return chromecast::media::mojom::SampleFormat::kSampleFormatF32;
      case (chromecast::media::SampleFormat::kSampleFormatPlanarU8):
        return chromecast::media::mojom::SampleFormat::kSampleFormatPlanarU8;
      case (chromecast::media::SampleFormat::kSampleFormatPlanarS16):
        return chromecast::media::mojom::SampleFormat::kSampleFormatPlanarS16;
      case (chromecast::media::SampleFormat::kSampleFormatPlanarF32):
        return chromecast::media::mojom::SampleFormat::kSampleFormatPlanarF32;
      case (chromecast::media::SampleFormat::kSampleFormatPlanarS32):
        return chromecast::media::mojom::SampleFormat::kSampleFormatPlanarS32;
      case (chromecast::media::SampleFormat::kSampleFormatS24):
        return chromecast::media::mojom::SampleFormat::kSampleFormatS24;
    }
    DLOG(FATAL) << "Unrecognized SampleFormat";
    return chromecast::media::mojom::SampleFormat::kUnknownSampleFormat;
  }

  static chromecast::media::SampleFormat FromMojom(
      chromecast::media::mojom::SampleFormat input) {
    switch (input) {
      case (chromecast::media::mojom::SampleFormat::kUnknownSampleFormat):
        return chromecast::media::SampleFormat::kUnknownSampleFormat;
      case (chromecast::media::mojom::SampleFormat::kSampleFormatU8):
        return chromecast::media::SampleFormat::kSampleFormatU8;
      case (chromecast::media::mojom::SampleFormat::kSampleFormatS16):
        return chromecast::media::SampleFormat::kSampleFormatS16;
      case (chromecast::media::mojom::SampleFormat::kSampleFormatS32):
        return chromecast::media::SampleFormat::kSampleFormatS32;
      case (chromecast::media::mojom::SampleFormat::kSampleFormatF32):
        return chromecast::media::SampleFormat::kSampleFormatF32;
      case (chromecast::media::mojom::SampleFormat::kSampleFormatPlanarU8):
        return chromecast::media::SampleFormat::kSampleFormatPlanarU8;
      case (chromecast::media::mojom::SampleFormat::kSampleFormatPlanarS16):
        return chromecast::media::SampleFormat::kSampleFormatPlanarS16;
      case (chromecast::media::mojom::SampleFormat::kSampleFormatPlanarF32):
        return chromecast::media::SampleFormat::kSampleFormatPlanarF32;
      case (chromecast::media::mojom::SampleFormat::kSampleFormatPlanarS32):
        return chromecast::media::SampleFormat::kSampleFormatPlanarS32;
      case (chromecast::media::mojom::SampleFormat::kSampleFormatS24):
        return chromecast::media::SampleFormat::kSampleFormatS24;
    }
    NOTREACHED();
  }
};

template <>
struct mojo::EnumTraits<chromecast::media::mojom::EncryptionScheme,
                        chromecast::media::EncryptionScheme> {
  static chromecast::media::mojom::EncryptionScheme ToMojom(
      chromecast::media::EncryptionScheme input) {
    switch (input) {
      case (chromecast::media::EncryptionScheme::kUnencrypted):
        return chromecast::media::mojom::EncryptionScheme::kUnencrypted;
      case (chromecast::media::EncryptionScheme::kAesCtr):
        return chromecast::media::mojom::EncryptionScheme::kAesCtr;
      case (chromecast::media::EncryptionScheme::kAesCbc):
        return chromecast::media::mojom::EncryptionScheme::kAesCbc;
    }
    DLOG(FATAL) << "Unrecognized EncryptionScheme";
    return chromecast::media::mojom::EncryptionScheme::kUnencrypted;
  }

  static chromecast::media::EncryptionScheme FromMojom(
      chromecast::media::mojom::EncryptionScheme input) {
    switch (input) {
      case (chromecast::media::mojom::EncryptionScheme::kUnencrypted):
        return chromecast::media::EncryptionScheme::kUnencrypted;
      case (chromecast::media::mojom::EncryptionScheme::kAesCtr):
        return chromecast::media::EncryptionScheme::kAesCtr;
      case (chromecast::media::mojom::EncryptionScheme::kAesCbc):
        return chromecast::media::EncryptionScheme::kAesCbc;
    }
    NOTREACHED();
  }
};

template <>
struct mojo::EnumTraits<chromecast::media::mojom::StreamId,
                        chromecast::media::StreamId> {
  static chromecast::media::mojom::StreamId ToMojom(
      chromecast::media::StreamId input) {
    switch (input) {
      case (chromecast::media::StreamId::kPrimary):
        return chromecast::media::mojom::StreamId::kPrimary;
      case (chromecast::media::StreamId::kSecondary):
        return chromecast::media::mojom::StreamId::kSecondary;
    }
    DLOG(FATAL) << "Unrecognized StreamId";
    return chromecast::media::mojom::StreamId::kPrimary;
  }

  static chromecast::media::StreamId FromMojom(
      chromecast::media::mojom::StreamId input) {
    switch (input) {
      case (chromecast::media::mojom::StreamId::kPrimary):
        return chromecast::media::StreamId::kPrimary;
      case (chromecast::media::mojom::StreamId::kSecondary):
        return chromecast::media::StreamId::kSecondary;
    }
    NOTREACHED();
  }
};

template <>
struct StructTraits<chromecast::media::mojom::AudioConfigDataView,
                    chromecast::media::AudioConfig> {
  static chromecast::media::StreamId id(
      const chromecast::media::AudioConfig& input) {
    return input.id;
  }

  static chromecast::media::AudioCodec codec(
      const chromecast::media::AudioConfig& input) {
    return input.codec;
  }

  static chromecast::media::ChannelLayout channel_layout(
      const chromecast::media::AudioConfig& input) {
    return input.channel_layout;
  }

  static chromecast::media::SampleFormat sample_format(
      const chromecast::media::AudioConfig& input) {
    return input.sample_format;
  }

  static int bytes_per_channel(const chromecast::media::AudioConfig& input) {
    return input.bytes_per_channel;
  }

  static int channel_number(const chromecast::media::AudioConfig& input) {
    return input.channel_number;
  }

  static int samples_per_second(const chromecast::media::AudioConfig& input) {
    return input.samples_per_second;
  }

  static chromecast::media::EncryptionScheme encryption_scheme(
      const chromecast::media::AudioConfig& input) {
    return input.encryption_scheme;
  }

  static const std::vector<uint8_t>& extra_data(
      const chromecast::media::AudioConfig& input) {
    return input.extra_data;
  }

  static bool Read(chromecast::media::mojom::AudioConfigDataView input,
                   chromecast::media::AudioConfig* output);
};

}  // namespace mojo

#endif  // CHROMECAST_MEDIA_MOJOM_DECODER_CONFIG_MOJOM_TRAITS_H_
