// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_BITSTREAM_AUDIO_CODECS_H_
#define CHROMECAST_BASE_BITSTREAM_AUDIO_CODECS_H_

#include <string>

namespace chromecast {

constexpr int kBitstreamAudioCodecNone = 0b000000;
constexpr int kBitstreamAudioCodecAc3 = 0b000001;
constexpr int kBitstreamAudioCodecDts = 0b000010;
constexpr int kBitstreamAudioCodecDtsHd = 0b000100;
constexpr int kBitstreamAudioCodecEac3 = 0b001000;
constexpr int kBitstreamAudioCodecPcmSurround = 0b010000;
constexpr int kBitstreamAudioCodecMpegHAudio = 0b100000;
constexpr int kBitstreamAudioCodecDtsXP2 = 0b1000000;
constexpr int kBitstreamAudioCodecAll = 0b1111111;

// Supported bitstream audio codecs and their associated properties.
struct BitstreamAudioCodecsInfo {
  // Bitmap of supported bitstream audio codecs.
  int codecs = kBitstreamAudioCodecNone;

  // Bitmap specifying which of the corresponding codecs in |codecs| support
  // spatial rendering.
  int spatial_rendering = kBitstreamAudioCodecNone;

  BitstreamAudioCodecsInfo operator&(
      const BitstreamAudioCodecsInfo& other) const;

  bool operator==(const BitstreamAudioCodecsInfo& other) const;
  bool operator!=(const BitstreamAudioCodecsInfo& other) const;

  BitstreamAudioCodecsInfo ApplyCodecMask(int mask) const;
};

std::string BitstreamAudioCodecsToString(int codecs);
std::string BitstreamAudioCodecsInfoToString(
    const BitstreamAudioCodecsInfo& info);

}  // namespace chromecast

#endif  // CHROMECAST_BASE_BITSTREAM_AUDIO_CODECS_H_
