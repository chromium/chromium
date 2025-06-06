// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/media/renderer/demuxer_stream_reader.h"

#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "chromecast/starboard/chromecast/starboard_cast_api/cast_starboard_api_types.h"
#include "chromecast/starboard/media/cdm/starboard_drm_key_tracker.h"
#include "chromecast/starboard/media/media/starboard_resampler.h"
#include "chromecast/starboard/media/renderer/chromium_starboard_conversions.h"
#include "media/base/video_decoder_config.h"

namespace chromecast {
namespace media {

namespace {

using ::media::DecoderBuffer;

scoped_refptr<DecoderBuffer> ConvertPcmAudioBufferToS16(
    ::media::AudioCodec codec,
    ::media::SampleFormat sample_format,
    int channel_count,
    scoped_refptr<DecoderBuffer> buffer) {
  return DecoderBuffer::FromArray(
      chromecast::media::ResamplePCMAudioDataForStarboard(
          StarboardPcmSampleFormat::kStarboardPcmSampleFormatS16, sample_format,
          codec, channel_count, *buffer));
}

// Function used to "convert" buffers that do not need to be converted.
scoped_refptr<DecoderBuffer> DoNotConvertBuffer(
    scoped_refptr<DecoderBuffer> buffer) {
  return buffer;
}

// Returns whether it is necessary to resample audio specified by `audio_config`
// to S16.
bool IsResamplingNecessary(const ::media::AudioDecoderConfig& audio_config) {
  return (audio_config.codec() == ::media::AudioCodec::kPCM &&
          audio_config.sample_format() !=
              ::media::SampleFormat::kSampleFormatS16) ||
         audio_config.codec() == ::media::AudioCodec::kPCM_S16BE ||
         audio_config.codec() == ::media::AudioCodec::kPCM_S24BE;
}

}  // namespace

DemuxerStreamReader::DemuxerStreamReader(
    ::media::DemuxerStream* audio_stream,
    ::media::DemuxerStream* video_stream,
    std::optional<StarboardAudioSampleInfo> audio_sample_info,
    std::optional<StarboardVideoSampleInfo> video_sample_info,
    HandleBufferCb handle_buffer_cb,
    HandleEosCb handle_eos_cb,
    ::media::RendererClient* client)
    : handle_buffer_cb_(std::move(handle_buffer_cb)),
      handle_eos_cb_(std::move(handle_eos_cb)),
      client_(client),
      audio_stream_(audio_stream),
      video_stream_(video_stream),
      audio_sample_info_(std::move(audio_sample_info)),
      video_sample_info_(std::move(video_sample_info)) {
  if (audio_stream_) {
    ::media::AudioDecoderConfig audio_config =
        audio_stream_->audio_decoder_config();

    if (IsResamplingNecessary(audio_config)) {
      convert_audio_fn_ = base::BindRepeating(
          &ConvertPcmAudioBufferToS16, audio_config.codec(),
          audio_config.sample_format(), audio_config.channels());
    } else {
      convert_audio_fn_ = base::BindRepeating(&DoNotConvertBuffer);
    }
  }
}

DemuxerStreamReader::~DemuxerStreamReader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const auto& [token, unused] : token_to_drm_key_cb_) {
    StarboardDrmKeyTracker::GetInstance().UnregisterCallback(token);
  }
}

void DemuxerStreamReader::ReadBuffer(int seek_ticket, StarboardMediaType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ::media::DemuxerStream* stream =
      type == StarboardMediaType::kStarboardMediaTypeAudio ? audio_stream_
                                                           : video_stream_;
  CHECK(stream);
  stream->Read(1,
               base::BindOnce(&DemuxerStreamReader::OnReadBuffer,
                              weak_factory_.GetWeakPtr(), type, seek_ticket));
}

void DemuxerStreamReader::HandleNonOkDemuxerStatus(
    ::media::DemuxerStream::Status status,
    StarboardMediaType type,
    int seek_ticket) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (status) {
    case ::media::DemuxerStream::Status::kAborted: {
      LOG(ERROR) << "DemuxerStream was aborted.";
      // This can happen if a flush occurs while we were trying to read from a
      // DemuxerStream. In that case, upstream code will call StartPlayingFrom
      // again, so we should not do another read here.
      return;
    }
    case ::media::DemuxerStream::Status::kConfigChanged: {
      if (type == StarboardMediaType::kStarboardMediaTypeAudio) {
        UpdateAudioConfig();

        // Keep reading more data.
        audio_stream_->Read(
            1, base::BindOnce(&DemuxerStreamReader::OnReadBuffer,
                              weak_factory_.GetWeakPtr(), type, seek_ticket));
      } else {
        CHECK_EQ(type, StarboardMediaType::kStarboardMediaTypeVideo);
        UpdateVideoConfig();

        // Keep reading more data.
        video_stream_->Read(
            1, base::BindOnce(&DemuxerStreamReader::OnReadBuffer,
                              weak_factory_.GetWeakPtr(), type, seek_ticket));
      }
      return;
    }
    case ::media::DemuxerStream::Status::kError:
      LOG(ERROR) << "DemuxerStream error occurred";
      client_->OnError(::media::PIPELINE_ERROR_READ);
      return;
    case ::media::DemuxerStream::Status::kOk:
      LOG(FATAL) << "OK status must be handled separately";
    default:
      LOG(WARNING) << "Received unknown DemuxerStream status: " << status
                   << ", with name "
                   << ::media::DemuxerStream::GetStatusName(status);
      return;
  }
}

void DemuxerStreamReader::OnReadBuffer(
    StarboardMediaType type,
    int seek_ticket,
    ::media::DemuxerStream::Status status,
    std::vector<scoped_refptr<DecoderBuffer>> buffers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != ::media::DemuxerStream::Status::kOk) {
    DCHECK(buffers.empty());
    HandleNonOkDemuxerStatus(status, type, seek_ticket);
    return;
  }

  CHECK_EQ(buffers.size(), 1UL);
  scoped_refptr<DecoderBuffer> buffer = std::move(buffers[0]);
  CHECK(buffer);

  if (buffer->end_of_stream()) {
    handle_eos_cb_.Run(seek_ticket, type);
    return;
  }

  if (type == StarboardMediaType::kStarboardMediaTypeAudio) {
    buffer = convert_audio_fn_.Run(std::move(buffer));
  }

  StarboardSampleInfo sample_info = {};
  sample_info.type = type;
  sample_info.buffer = buffer->data();
  sample_info.buffer_size = buffer->size();
  sample_info.timestamp = buffer->timestamp().InMicroseconds();
  sample_info.side_data = base::span<const StarboardSampleSideData>();

  if (type == StarboardMediaType::kStarboardMediaTypeAudio) {
    DCHECK(audio_sample_info_);
    sample_info.audio_sample_info = *audio_sample_info_;
  } else {
    DCHECK(video_sample_info_);
    sample_info.video_sample_info = *video_sample_info_;
    // is_key_frame needs to be set per sample.
    sample_info.video_sample_info.is_key_frame = buffer->is_key_frame();

    if (first_video_frame_) {
      first_video_frame_ = false;
      client_->OnVideoNaturalSizeChange(
          gfx::Size(sample_info.video_sample_info.frame_width,
                    sample_info.video_sample_info.frame_height));
    }
  }

  // drm_info must not be deleted until after sample_info is passed to
  // starboard.
  DrmInfoWrapper drm_info = DrmInfoWrapper::Create(*buffer);
  sample_info.drm_info = drm_info.GetDrmSampleInfo();

  // For encrypted buffers, we should not push data to starboard util the
  // buffer's DRM key is available to the CDM. To accomplish this, we check with
  // the StarboardDrmKeyTracker singleton -- which is updated by the CDM,
  // StarboardDecryptorCast -- to see whether the key is available. If the key
  // is not available yet, we register a callback that will be run once the key
  // becomes available.
  if (sample_info.drm_info) {
    const std::string drm_key(
        reinterpret_cast<const char*>(sample_info.drm_info->identifier),
        sample_info.drm_info->identifier_size);
    if (!StarboardDrmKeyTracker::GetInstance().HasKey(drm_key)) {
      WaitForKey(std::move(drm_info), std::move(sample_info), std::move(buffer),
                 seek_ticket);
      return;
    }

    // The key is already available; continue the logic of pushing the buffer to
    // starboard.
  }

  handle_buffer_cb_.Run(seek_ticket, std::move(sample_info), std::move(buffer));
}

void DemuxerStreamReader::WaitForKey(DrmInfoWrapper drm_info,
                                     StarboardSampleInfo sample_info,
                                     scoped_refptr<DecoderBuffer> buffer,
                                     int seek_ticket) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  StarboardDrmSampleInfo* const drm_sample_info = drm_info.GetDrmSampleInfo();
  const std::string drm_key(
      reinterpret_cast<const char*>(drm_sample_info->identifier),
      drm_sample_info->identifier_size);

  const size_t key_hash = base::FastHash(
      base::span(drm_sample_info->identifier)
          .first(static_cast<size_t>(drm_sample_info->identifier_size)));
  LOG(INFO) << "Waiting for DRM key with hash: " << key_hash;
  CHECK(base::SequencedTaskRunner::HasCurrentDefault());
  const int64_t token = StarboardDrmKeyTracker::GetInstance().WaitForKey(
      drm_key,
      base::BindPostTask(
          base::SequencedTaskRunner::GetCurrentDefault(),
          base::BindOnce(&DemuxerStreamReader::RunPendingDrmKeyCallback,
                         weak_factory_.GetWeakPtr())));

  CHECK(!token_to_drm_key_cb_.contains(token))
      << "Got duplicate DRM key token: " << token;

  // Bind the buffer to a closure that will be run when the DRM key is
  // available.
  base::OnceClosure handle_buffer_closure =
      base::BindOnce(handle_buffer_cb_, seek_ticket, std::move(sample_info),
                     std::move(buffer));

  // Bind the DrmInfoWrapper as well, so that it outlives handle_buffer_closure.
  token_to_drm_key_cb_[token] = base::BindOnce(
      [](base::OnceClosure cb, DrmInfoWrapper drm_info) {
        // drm_info must outlive this call. Otherwise, the pointers in the
        // sample_info could point to bad memory.
        std::move(cb).Run();
      },
      std::move(handle_buffer_closure), std::move(drm_info));

  client_->OnWaiting(::media::WaitingReason::kNoDecryptionKey);
}

void DemuxerStreamReader::RunPendingDrmKeyCallback(int64_t token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (auto it = token_to_drm_key_cb_.find(token);
      it != token_to_drm_key_cb_.end()) {
    std::move(it->second).Run();
    token_to_drm_key_cb_.erase(it);
  }
}

void DemuxerStreamReader::UpdateAudioConfig() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(audio_stream_);
  chromium_audio_config_ = audio_stream_->audio_decoder_config();
  LOG(INFO) << "Audio config changed to "
            << chromium_audio_config_.AsHumanReadableString();
  audio_sample_info_ = ToStarboardAudioSampleInfo(chromium_audio_config_);
  if (IsResamplingNecessary(chromium_audio_config_)) {
    convert_audio_fn_ = base::BindRepeating(
        &ConvertPcmAudioBufferToS16, chromium_audio_config_.codec(),
        chromium_audio_config_.sample_format(),
        chromium_audio_config_.channels());
  } else {
    convert_audio_fn_ = base::BindRepeating(&DoNotConvertBuffer);
  }
  client_->OnAudioConfigChange(chromium_audio_config_);
}

void DemuxerStreamReader::UpdateVideoConfig() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(video_stream_);
  const ::media::VideoDecoderConfig video_config =
      video_stream_->video_decoder_config();
  LOG(INFO) << "Video config changed to "
            << video_config.AsHumanReadableString();
  std::optional<StarboardVideoSampleInfo> new_sample_info =
      ToStarboardVideoSampleInfo(video_config);

  // TODO(antoniori): maybe fail gracefully here, rather than crashing.
  DCHECK(new_sample_info);
  if (!video_sample_info_ ||
      video_sample_info_->frame_width != new_sample_info->frame_width ||
      video_sample_info_->frame_height != new_sample_info->frame_height) {
    client_->OnVideoNaturalSizeChange(gfx::Size(
        video_sample_info_->frame_width, video_sample_info_->frame_height));
  }
  video_sample_info_ = std::move(new_sample_info);
  client_->OnVideoConfigChange(video_config);
}

}  // namespace media
}  // namespace chromecast
