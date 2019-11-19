// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_MEDIA_DECODER_CONFIG_H_
#define CHROMECAST_PUBLIC_MEDIA_DECODER_CONFIG_H_

#include <stdint.h>
#include <vector>

#include "cast_decrypt_config.h"
#include "stream_id.h"

namespace chromecast {
namespace media {

// Maximum audio bytes per sample.
static const int kMaxBytesPerSample = 4;

// Maximum audio sampling rate.
static const int kMaxSampleRate = 192000;

// TODO(guohuideng): change at least AudioCodec and SampleFormat to enum class.
enum AudioCodec : int {
  kAudioCodecUnknown = 0,
  kCodecAAC,
  kCodecMP3,
  kCodecPCM,
  kCodecPCM_S16BE,
  kCodecVorbis,
  kCodecOpus,
  kCodecEAC3,
  kCodecAC3,
  kCodecDTS,
  kCodecFLAC,
  kCodecMpegHAudio,

  kAudioCodecMin = kAudioCodecUnknown,
  kAudioCodecMax = kCodecMpegHAudio,
};

enum class ChannelLayout {
  UNSUPPORTED,

  // Front C
  MONO,

  // Front L, Front R
  STEREO,

  // Front L, Front R, Front C, LFE, Side L, Side R
  SURROUND_5_1,

  // Actual channel layout is specified in the bitstream and the actual channel
  // count is unknown at Chromium media pipeline level (useful for audio
  // pass-through mode).
  BITSTREAM,

  // Max value, must always equal the largest entry ever logged.
  MAX_LAST = BITSTREAM,
};

// Internal chromecast apps use this to decide on channel_layout.
inline ChannelLayout ChannelLayoutFromChannelNumber(int channel_number) {
  switch (channel_number) {
    case 1:
      return ChannelLayout::MONO;
    case 2:
      return ChannelLayout::STEREO;
    case 6:
      return ChannelLayout::SURROUND_5_1;
    default:
      return ChannelLayout::UNSUPPORTED;
  }
}

enum SampleFormat : int {
  kUnknownSampleFormat = 0,
  kSampleFormatU8,         // Unsigned 8-bit w/ bias of 128.
  kSampleFormatS16,        // Signed 16-bit.
  kSampleFormatS32,        // Signed 32-bit.
  kSampleFormatF32,        // Float 32-bit.
  kSampleFormatPlanarS16,  // Signed 16-bit planar.
  kSampleFormatPlanarF32,  // Float 32-bit planar.
  kSampleFormatPlanarS32,  // Signed 32-bit planar.
  kSampleFormatS24,        // Signed 24-bit.

  kSampleFormatMin = kUnknownSampleFormat,
  kSampleFormatMax = kSampleFormatS24,
};

enum VideoCodec : int {
  kVideoCodecUnknown = 0,
  kCodecH264,
  kCodecVC1,
  kCodecMPEG2,
  kCodecMPEG4,
  kCodecTheora,
  kCodecVP8,
  kCodecVP9,
  kCodecHEVC,
  kCodecDolbyVisionH264,
  kCodecDolbyVisionHEVC,
  kCodecAV1,

  kVideoCodecMin = kVideoCodecUnknown,
  kVideoCodecMax = kCodecAV1,
};

// Profile for Video codec.
enum VideoProfile : int {
  kVideoProfileUnknown = 0,
  kH264Baseline,
  kH264Main,
  kH264Extended,
  kH264High,
  kH264High10,
  kH264High422,
  kH264High444Predictive,
  kH264ScalableBaseline,
  kH264ScalableHigh,
  kH264StereoHigh,
  kH264MultiviewHigh,
  kVP8ProfileAny,
  kVP9Profile0,
  kVP9Profile1,
  kVP9Profile2,
  kVP9Profile3,
  kDolbyVisionCompatible_EL_MD,
  kDolbyVisionCompatible_BL_EL_MD,
  kDolbyVisionNonCompatible_BL_MD,
  kDolbyVisionNonCompatible_BL_EL_MD,
  kHEVCMain,
  kHEVCMain10,
  kHEVCMainStillPicture,
  kAV1ProfileMain,
  kAV1ProfileHigh,
  kAV1ProfilePro,

  kVideoProfileMin = kVideoProfileUnknown,
  kVideoProfileMax = kAV1ProfilePro,
};

struct CodecProfileLevel {
  VideoCodec codec;
  VideoProfile profile;
  int level;
};

// ---- Begin copy/paste from //media/base/video_color_space.h ----
// Described in ISO 23001-8:2016

// Table 2
enum class PrimaryID : uint8_t {
  INVALID = 0,
  BT709 = 1,
  UNSPECIFIED = 2,
  BT470M = 4,
  BT470BG = 5,
  SMPTE170M = 6,
  SMPTE240M = 7,
  FILM = 8,
  BT2020 = 9,
  SMPTEST428_1 = 10,
  SMPTEST431_2 = 11,
  SMPTEST432_1 = 12,
  EBU_3213_E = 22
};

// Table 3
enum class TransferID : uint8_t {
  INVALID = 0,
  BT709 = 1,
  UNSPECIFIED = 2,
  GAMMA22 = 4,
  GAMMA28 = 5,
  SMPTE170M = 6,
  SMPTE240M = 7,
  LINEAR = 8,
  LOG = 9,
  LOG_SQRT = 10,
  IEC61966_2_4 = 11,
  BT1361_ECG = 12,
  IEC61966_2_1 = 13,
  BT2020_10 = 14,
  BT2020_12 = 15,
  SMPTEST2084 = 16,
  SMPTEST428_1 = 17,

  // Not yet standardized
  ARIB_STD_B67 = 18,  // AKA hybrid-log gamma, HLG.
};

// Table 4
enum class MatrixID : uint8_t {
  RGB = 0,
  BT709 = 1,
  UNSPECIFIED = 2,
  FCC = 4,
  BT470BG = 5,
  SMPTE170M = 6,
  SMPTE240M = 7,
  YCOCG = 8,
  BT2020_NCL = 9,
  BT2020_CL = 10,
  YDZDX = 11,
  INVALID = 255,
};
// ---- End copy/pasted from media/base/video_color_space.h ----

// This corresponds to the WebM Range enum which is part of WebM color data
// (see http://www.webmproject.org/docs/container/#Range).
// H.264 only uses a bool, which corresponds to the LIMITED/FULL values.
// ---- Begin copy/paste from //ui/gfx/color_space.h ----
enum class RangeID : int8_t {
  INVALID = 0,
  // Limited Rec. 709 color range with RGB values ranging from 16 to 235.
  LIMITED = 1,
  // Full RGB color range with RGB valees from 0 to 255.
  FULL = 2,
  // Range is defined by TransferID/MatrixID.
  DERIVED = 3,
  LAST = DERIVED
};
// ---- Begin copy/paste from //ui/gfx/color_space.h ----

// ---- Begin copy/paste from media/base/hdr_metadata.h ----
// SMPTE ST 2086 mastering metadata.
struct MasteringMetadata {
  float primary_r_chromaticity_x = 0;
  float primary_r_chromaticity_y = 0;
  float primary_g_chromaticity_x = 0;
  float primary_g_chromaticity_y = 0;
  float primary_b_chromaticity_x = 0;
  float primary_b_chromaticity_y = 0;
  float white_point_chromaticity_x = 0;
  float white_point_chromaticity_y = 0;
  float luminance_max = 0;
  float luminance_min = 0;

  MasteringMetadata();
  MasteringMetadata(const MasteringMetadata& rhs);
};

// HDR metadata common for HDR10 and WebM/VP9-based HDR formats.
struct HDRMetadata {
  MasteringMetadata mastering_metadata;
  unsigned max_content_light_level = 0;
  unsigned max_frame_average_light_level = 0;

  HDRMetadata();
  HDRMetadata(const HDRMetadata& rhs);
};

inline MasteringMetadata::MasteringMetadata() {}
inline MasteringMetadata::MasteringMetadata(const MasteringMetadata& rhs) =
    default;

inline HDRMetadata::HDRMetadata() {}
inline HDRMetadata::HDRMetadata(const HDRMetadata& rhs) = default;
// ---- End copy/paste from media/base/hdr_metadata.h ----

constexpr int kChannelAll = -1;

// TODO(erickung): Remove constructor once CMA backend implementation doesn't
// create a new object to reset the configuration and use IsValidConfig() to
// determine if the configuration is still valid or not.
struct AudioConfig {
  AudioConfig();
  AudioConfig(const AudioConfig& other);
  ~AudioConfig();

  bool is_encrypted() const {
    return encryption_scheme != EncryptionScheme::kUnencrypted;
  }

  // Stream id.
  StreamId id;
  // Audio codec.
  AudioCodec codec;
  // Audio channel layout.
  ChannelLayout channel_layout;
  // The format of each audio sample.
  SampleFormat sample_format;
  // Number of bytes in each channel.
  int bytes_per_channel;
  // Number of channels in this audio stream.
  int channel_number;
  // Number of audio samples per second.
  int samples_per_second;
  // Extra data buffer for certain codec initialization.
  std::vector<uint8_t> extra_data;
  // Encryption scheme (if any) used for the content.
  EncryptionScheme encryption_scheme;
};

inline AudioConfig::AudioConfig()
    : id(kPrimary),
      codec(kAudioCodecUnknown),
      channel_layout(ChannelLayout::UNSUPPORTED),
      sample_format(kUnknownSampleFormat),
      bytes_per_channel(0),
      channel_number(0),
      samples_per_second(0),
      encryption_scheme(EncryptionScheme::kUnencrypted) {}
inline AudioConfig::AudioConfig(const AudioConfig& other) = default;
inline AudioConfig::~AudioConfig() {
}

// TODO(erickung): Remove constructor once CMA backend implementation does't
// create a new object to reset the configuration and use IsValidConfig() to
// determine if the configuration is still valid or not.
struct VideoConfig {
  VideoConfig();
  VideoConfig(const VideoConfig& other);
  ~VideoConfig();

  bool is_encrypted() const {
    return encryption_scheme != EncryptionScheme::kUnencrypted;
  }

  // Stream Id.
  StreamId id;
  // Video codec.
  VideoCodec codec;
  // Video codec profile.
  VideoProfile profile;
  // Additional video config for the video stream if available. Consumers of
  // this structure should make an explicit copy of |additional_config| if it
  // will be used after SetConfig() finishes.
  VideoConfig* additional_config;
  // Extra data buffer for certain codec initialization.
  std::vector<uint8_t> extra_data;
  // Encryption scheme (if any) used for the content.
  EncryptionScheme encryption_scheme;

  // ColorSpace info
  PrimaryID primaries = PrimaryID::UNSPECIFIED;
  TransferID transfer = TransferID::UNSPECIFIED;
  MatrixID matrix = MatrixID::UNSPECIFIED;
  RangeID range = RangeID::INVALID;

  bool have_hdr_metadata = false;
  HDRMetadata hdr_metadata;
};

inline VideoConfig::VideoConfig()
    : id(kPrimary),
      codec(kVideoCodecUnknown),
      profile(kVideoProfileUnknown),
      additional_config(nullptr),
      encryption_scheme(EncryptionScheme::kUnencrypted) {}

inline VideoConfig::VideoConfig(const VideoConfig& other) = default;

inline VideoConfig::~VideoConfig() {
}

inline bool IsValidConfig(const AudioConfig& config) {
  return config.codec >= kAudioCodecMin && config.codec <= kAudioCodecMax &&
         config.codec != kAudioCodecUnknown &&
         config.channel_layout != ChannelLayout::UNSUPPORTED &&
         config.sample_format >= kSampleFormatMin &&
         config.sample_format <= kSampleFormatMax &&
         config.sample_format != kUnknownSampleFormat &&
         (config.channel_number > 0 ||
          config.channel_layout == ChannelLayout::BITSTREAM) &&
         config.bytes_per_channel > 0 &&
         config.bytes_per_channel <= kMaxBytesPerSample &&
         config.samples_per_second > 0 &&
         config.samples_per_second <= kMaxSampleRate;
}

inline bool IsValidConfig(const VideoConfig& config) {
  return config.codec >= kVideoCodecMin &&
      config.codec <= kVideoCodecMax &&
      config.codec != kVideoCodecUnknown;
}

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_MEDIA_DECODER_CONFIG_H_
