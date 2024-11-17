// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "starboard_audio_decoder.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>

#include "base/check.h"
#include "base/logging.h"
#include "chromecast/public/media/cast_decoder_buffer.h"
#include "chromecast/starboard/chromecast/starboard_cast_api/cast_starboard_api_types.h"
#include "chromecast/starboard/media/media/drm_util.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"

namespace chromecast {
namespace media {

using BufferStatus = ::chromecast::media::MediaPipelineBackend::BufferStatus;
using RenderingDelay =
    ::chromecast::media::MediaPipelineBackend::AudioDecoder::RenderingDelay;

static StarboardAudioCodec AudioCodecToStarboardCodec(AudioCodec codec) {
  switch (codec) {
    case kCodecAAC:
      return kStarboardAudioCodecAac;
    case kCodecMP3:
      return kStarboardAudioCodecMp3;
    case kCodecPCM_S16BE:
    case kCodecPCM:
      return kStarboardAudioCodecPcm;
    case kCodecVorbis:
      return kStarboardAudioCodecVorbis;
    case kCodecOpus:
      return kStarboardAudioCodecOpus;
    case kCodecEAC3:
      return kStarboardAudioCodecEac3;
    case kCodecAC3:
      return kStarboardAudioCodecAc3;
    case kCodecFLAC:
      return kStarboardAudioCodecFlac;
    case kCodecMpegHAudio:
    case kCodecDTS:
    case kAudioCodecUnknown:
    default:
      LOG(ERROR) << "Unsupported audio codec: " << codec;
      return kStarboardAudioCodecNone;
  }
}

static StarboardAudioSampleInfo ToAudioSampleInfo(const AudioConfig& config) {
  StarboardAudioSampleInfo sample_info = {};

  sample_info.codec = AudioCodecToStarboardCodec(config.codec);
  sample_info.mime = "";
  sample_info.format_tag = 0;
  sample_info.number_of_channels = config.channel_number;
  sample_info.samples_per_second = config.samples_per_second;
  // Based on starboard_utils.cc (MediaAudioConfigToSbMediaAudioSampleInfo) in
  // the cobalt codebase, this value does not take into account the number of
  // channels.
  // TODO(b/334907387): Add logic to change audio_sample_info_.bits_per_sample
  // depending on our desired output. For now it's just 16 because we only
  // need signed 16 as our desired out.
  if (sample_info.codec == kStarboardAudioCodecPcm) {
    sample_info.bits_per_sample = 16;
  } else {
    sample_info.bits_per_sample = config.bytes_per_channel * 8;
  }
  sample_info.block_alignment = 4;
  sample_info.average_bytes_per_second = sample_info.number_of_channels *
                                         sample_info.samples_per_second *
                                         sample_info.bits_per_sample / 8;
  // Since extra_data is a vector of uint8_t, size() gives us the size in bytes.
  sample_info.audio_specific_config_size = config.extra_data.size();
  sample_info.audio_specific_config =
      sample_info.audio_specific_config_size > 0
          ? static_cast<const void*>(config.extra_data.data())
          : nullptr;

  return sample_info;
}

StarboardAudioDecoder::StarboardAudioDecoder(StarboardApiWrapper* starboard)
    : StarboardDecoder(starboard, kStarboardMediaTypeAudio),
      format_to_decode_to_(
          StarboardPcmSampleFormat::kStarboardPcmSampleFormatS16) {}

StarboardAudioDecoder::~StarboardAudioDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void StarboardAudioDecoder::InitializeInternal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (volume_) {
    LOG(INFO) << "Setting starboard's volume to " << *volume_;
    GetStarboardApi().SetVolume(GetPlayer(), *volume_);
    volume_ = std::nullopt;
  }
}

const std::optional<StarboardAudioSampleInfo>&
StarboardAudioDecoder::GetAudioSampleInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return audio_sample_info_;
}

bool StarboardAudioDecoder::SetConfig(const AudioConfig& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if ((config.codec == kCodecPCM || config.codec == kCodecPCM_S16BE) &&
      config.channel_number > 8) {
    LOG(ERROR) << "Config channels exceeds 8, which is not supported.";
    return false;
  }

  config_ = config;
  audio_sample_info_.emplace(ToAudioSampleInfo(config_));
  return IsValidConfig(config_);
}

bool StarboardAudioDecoder::SetVolume(float multiplier) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (multiplier < 0.0 || multiplier > 1.0) {
    LOG(ERROR) << "Invalid volume multiplier: " << multiplier;
    return false;
  }

  void* const player = GetPlayer();
  if (player) {
    LOG(INFO) << "Setting starboard's volume to " << multiplier;
    GetStarboardApi().SetVolume(player, multiplier);
  } else {
    LOG(INFO) << "Delaying setting volume until SbPlayer is created.";
    volume_ = multiplier;
  }

  return true;
}

RenderingDelay StarboardAudioDecoder::GetRenderingDelay() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RenderingDelay delay = {};
  // Signifies that the latency is not available.
  delay.timestamp_microseconds = std::numeric_limits<int64_t>::min();
  delay.delay_microseconds = 0;
  return delay;
}

BufferStatus StarboardAudioDecoder::PushBuffer(CastDecoderBuffer* buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(buffer);

  if (buffer->end_of_stream()) {
    return PushEndOfStream();
  }

  DCHECK(audio_sample_info_);
  size_t size_of_buffer = buffer->data_size();
  std::unique_ptr<uint8_t[]> data_copy;

  if (audio_sample_info_->codec == kStarboardAudioCodecPcm) {
    // This call will also set the value for `size_of_buffer`.
    data_copy = ResamplePCMAudioDataForStarboard(
        format_to_decode_to_, config_.sample_format, config_.codec,
        audio_sample_info_->number_of_channels, *buffer, size_of_buffer);
  } else {
    // Need to do this when not resampling to ensure that the input data is not
    // freed until Starboard is done using it.
    data_copy = std::make_unique<uint8_t[]>(size_of_buffer);
    memcpy(data_copy.get(), buffer->data(), size_of_buffer);
  }

  StarboardSampleInfo sample = {};
  sample.type = kStarboardMediaTypeAudio;
  sample.timestamp = buffer->timestamp();
  sample.side_data = nullptr;
  sample.side_data_count = 0;
  sample.audio_sample_info = *audio_sample_info_;

  decoded_bytes_ += size_of_buffer;

  return PushBufferInternal(std::move(sample), GetDrmInfo(*buffer),
                            std::move(data_copy), size_of_buffer);
}

void StarboardAudioDecoder::GetStatistics(Statistics* statistics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!statistics) {
    return;
  }

  statistics->decoded_bytes = decoded_bytes_;
}

void StarboardAudioDecoder::SetDelegate(Delegate* delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetDecoderDelegate(delegate);
}

StarboardAudioDecoder::AudioTrackTimestamp
StarboardAudioDecoder::GetAudioTrackTimestamp() {
  return AudioTrackTimestamp();
}

int StarboardAudioDecoder::GetStartThresholdInFrames() {
  return 0;
}

}  // namespace media
}  // namespace chromecast
