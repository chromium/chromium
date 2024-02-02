// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/bitstream_audio_codecs.h"

#include <string_view>
#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"

namespace chromecast {

namespace {

const char* BitstreamAudioCodecToString(int codec) {
  switch (codec) {
    case kBitstreamAudioCodecNone:
      return "None";
    case kBitstreamAudioCodecAc3:
      return "AC3";
    case kBitstreamAudioCodecDts:
      return "DTS";
    case kBitstreamAudioCodecDtsHd:
      return "DTS-HD";
    case kBitstreamAudioCodecEac3:
      return "EAC3";
    case kBitstreamAudioCodecPcmSurround:
      return "PCM";
    case kBitstreamAudioCodecMpegHAudio:
      return "MPEG-H Audio";
    default:
      return "";
  }
}

}  // namespace

BitstreamAudioCodecsInfo BitstreamAudioCodecsInfo::operator&(
    const BitstreamAudioCodecsInfo& other) const {
  return BitstreamAudioCodecsInfo{codecs & other.codecs,
                                  spatial_rendering & other.spatial_rendering};
}

bool BitstreamAudioCodecsInfo::operator==(
    const BitstreamAudioCodecsInfo& other) const {
  return codecs == other.codecs && spatial_rendering == other.spatial_rendering;
}

bool BitstreamAudioCodecsInfo::operator!=(
    const BitstreamAudioCodecsInfo& other) const {
  return !(*this == other);
}

BitstreamAudioCodecsInfo BitstreamAudioCodecsInfo::ApplyCodecMask(
    int mask) const {
  return BitstreamAudioCodecsInfo{codecs & mask, spatial_rendering & mask};
}

std::string BitstreamAudioCodecsToString(int codecs) {
  std::string codec_string = BitstreamAudioCodecToString(codecs);
  if (!codec_string.empty()) {
    return codec_string;
  }
  std::vector<std::string_view> codec_strings;
  for (int codec :
       {kBitstreamAudioCodecAc3, kBitstreamAudioCodecDts,
        kBitstreamAudioCodecDtsHd, kBitstreamAudioCodecEac3,
        kBitstreamAudioCodecPcmSurround, kBitstreamAudioCodecMpegHAudio}) {
    if ((codec & codecs) != 0) {
      codec_strings.push_back(BitstreamAudioCodecToString(codec));
    }
  }
  return "[" + base::JoinString(codec_strings, ", ") + "]";
}

std::string BitstreamAudioCodecsInfoToString(
    const BitstreamAudioCodecsInfo& info) {
  return base::StrCat({"[codecs]", BitstreamAudioCodecsToString(info.codecs),
                       "[spatial_rendering]",
                       BitstreamAudioCodecsToString(info.spatial_rendering)});
}

}  // namespace chromecast
