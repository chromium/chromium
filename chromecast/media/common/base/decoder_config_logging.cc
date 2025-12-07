// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/common/base/decoder_config_logging.h"

#include "base/notreached.h"

std::ostream& operator<<(std::ostream& stream,
                         ::chromecast::media::AudioCodec codec) {
  switch (codec) {
    case ::chromecast::media::kAudioCodecUnknown:
      return stream << "unknown";
    case ::chromecast::media::kCodecAAC:
      return stream << "AAC";
    case ::chromecast::media::kCodecMP3:
      return stream << "MP3";
    case ::chromecast::media::kCodecPCM:
      return stream << "PCM";
    case ::chromecast::media::kCodecPCM_S16BE:
      return stream << "PCM_S16BE";
    case ::chromecast::media::kCodecVorbis:
      return stream << "Vorbis";
    case ::chromecast::media::kCodecOpus:
      return stream << "Opus";
    case ::chromecast::media::kCodecEAC3:
      return stream << "EAC3";
    case ::chromecast::media::kCodecAC3:
      return stream << "AC3";
    case ::chromecast::media::kCodecDTS:
      return stream << "DTS";
    case ::chromecast::media::kCodecDTSXP2:
      return stream << "DTS:X Profile 2";
    case ::chromecast::media::kCodecDTSE:
      return stream << "DTS Express";
    case ::chromecast::media::kCodecFLAC:
      return stream << "FLAC";
    case ::chromecast::media::kCodecMpegHAudio:
      return stream << "MPEG-H Audio";
  }
  NOTREACHED();
}

std::ostream& operator<<(std::ostream& stream,
                         ::chromecast::media::SampleFormat format) {
  switch (format) {
    case ::chromecast::media::kUnknownSampleFormat:
      return stream << "unknown";
    case ::chromecast::media::kSampleFormatU8:
      return stream << "interleaved unsigned 8-bit int";
    case ::chromecast::media::kSampleFormatS16:
      return stream << "interleaved signed 16-bit int";
    case ::chromecast::media::kSampleFormatS32:
      return stream << "interleaved signed 32-bit int";
    case ::chromecast::media::kSampleFormatF32:
      return stream << "interleaved float";
    case ::chromecast::media::kSampleFormatPlanarS16:
      return stream << "planar signed 16-bit int";
    case ::chromecast::media::kSampleFormatPlanarF32:
      return stream << "planar float";
    case ::chromecast::media::kSampleFormatPlanarS32:
      return stream << "planar signed 32-bit int";
    case ::chromecast::media::kSampleFormatS24:
      return stream << "interleaved signed 24-bit int";
    case ::chromecast::media::kSampleFormatPlanarU8:
      return stream << "planar unsigned 8-bit int";
  }
  NOTREACHED();
}
