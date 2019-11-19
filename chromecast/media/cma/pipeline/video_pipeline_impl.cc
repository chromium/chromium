// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/pipeline/video_pipeline_impl.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/media/cdm/cast_cdm_context.h"
#include "chromecast/media/cma/base/buffering_defs.h"
#include "chromecast/media/cma/base/coded_frame_provider.h"
#include "chromecast/media/cma/base/decoder_config_adapter.h"
#include "chromecast/media/cma/pipeline/av_pipeline_impl.h"
#include "chromecast/media/cma/pipeline/cdm_decryptor.h"
#include "chromecast/public/graphics_types.h"
#include "chromecast/public/media/decoder_config.h"
#include "media/base/video_decoder_config.h"

namespace chromecast {
namespace media {

namespace {
const size_t kMaxVideoFrameSize = 1024 * 1024;
}

VideoPipelineImpl::VideoPipelineImpl(CmaBackend::VideoDecoder* decoder,
                                     const VideoPipelineClient& client)
    : AvPipelineImpl(decoder, client.av_pipeline_client),
      video_decoder_(decoder),
      natural_size_changed_cb_(client.natural_size_changed_cb) {
  DCHECK(video_decoder_);
}

VideoPipelineImpl::~VideoPipelineImpl() {
}

::media::PipelineStatus VideoPipelineImpl::Initialize(
    const std::vector<::media::VideoDecoderConfig>& configs,
    std::unique_ptr<CodedFrameProvider> frame_provider) {
  DCHECK_GT(configs.size(), 0u);
  for (const auto& config : configs) {
    LOG(INFO) << __FUNCTION__ << " " << config.AsHumanReadableString();
  }

  if (frame_provider) {
    SetCodedFrameProvider(std::move(frame_provider), kAppVideoBufferSize,
                          kMaxVideoFrameSize);
  }

  if (configs.empty()) {
    return ::media::PIPELINE_ERROR_INITIALIZATION_FAILED;
  }
  DCHECK_LE(configs.size(), 2U);
  DCHECK(configs[0].IsValidConfig());
  encryption_schemes_.resize(configs.size());

  VideoConfig video_config =
      DecoderConfigAdapter::ToCastVideoConfig(kPrimary, configs[0]);
  encryption_schemes_[0] = video_config.encryption_scheme;

  VideoConfig secondary_config;
  if (configs.size() == 2) {
    DCHECK(configs[1].IsValidConfig());
    secondary_config = DecoderConfigAdapter::ToCastVideoConfig(kSecondary,
                                                               configs[1]);
    video_config.additional_config = &secondary_config;
    encryption_schemes_[1] = secondary_config.encryption_scheme;
  }

  if (!video_decoder_->SetConfig(video_config)) {
    return ::media::PIPELINE_ERROR_INITIALIZATION_FAILED;
  }

  set_state(kFlushed);
  return ::media::PIPELINE_OK;
}

void VideoPipelineImpl::OnVideoResolutionChanged(const Size& size) {
  if (state() != kPlaying)
    return;

  metrics::CastMetricsHelper* metrics_helper =
      metrics::CastMetricsHelper::GetInstance();
  int encoded_video_resolution = (size.width << 16) | size.height;
  metrics_helper->RecordApplicationEventWithValue(
      "Cast.Platform.VideoResolution", encoded_video_resolution);

  if (!natural_size_changed_cb_.is_null()) {
    natural_size_changed_cb_.Run(gfx::Size(size.width, size.height));
  }

  CastCdmContext* cdm = cdm_context();
  if (cdm) {
    cdm->SetVideoResolution(size.width, size.height);
  }
}

void VideoPipelineImpl::OnUpdateConfig(
    StreamId id,
    const ::media::AudioDecoderConfig& audio_config,
    const ::media::VideoDecoderConfig& video_config) {
  if (video_config.IsValidConfig()) {
    LOG(INFO) << __FUNCTION__ << " id:" << id << " "
              << video_config.AsHumanReadableString();

    DCHECK_LT(id, encryption_schemes_.size());
    VideoConfig cast_video_config =
        DecoderConfigAdapter::ToCastVideoConfig(id, video_config);
    encryption_schemes_[static_cast<int>(id)] =
        cast_video_config.encryption_scheme;

    bool success = video_decoder_->SetConfig(cast_video_config);
    if (!success && !client().playback_error_cb.is_null())
      client().playback_error_cb.Run(::media::PIPELINE_ERROR_DECODE);
  }
}

EncryptionScheme VideoPipelineImpl::GetEncryptionScheme(StreamId id) const {
  DCHECK_LT(id, encryption_schemes_.size());
  return encryption_schemes_[static_cast<int>(id)];
}

std::unique_ptr<StreamDecryptor> VideoPipelineImpl::CreateDecryptor() {
  return std::make_unique<CdmDecryptor>(false /* clear_buffer_needed */);
}

void VideoPipelineImpl::UpdateStatistics() {
  if (client().statistics_cb.is_null())
    return;

  // TODO(mbjorge): Give Statistics a default constructor when the
  // next system update happens. b/32802298
  CmaBackend::VideoDecoder::Statistics video_stats = {};
  video_decoder_->GetStatistics(&video_stats);

  ::media::PipelineStatistics current_stats;
  current_stats.video_bytes_decoded = video_stats.decoded_bytes;
  current_stats.video_frames_decoded = video_stats.decoded_frames;
  current_stats.video_frames_dropped = video_stats.dropped_frames;

  ::media::PipelineStatistics delta_stats;
  delta_stats.video_bytes_decoded =
      current_stats.video_bytes_decoded - previous_stats_.video_bytes_decoded;
  delta_stats.video_frames_decoded =
      current_stats.video_frames_decoded - previous_stats_.video_frames_decoded;
  delta_stats.video_frames_dropped =
      current_stats.video_frames_dropped - previous_stats_.video_frames_dropped;

  bytes_decoded_since_last_update_ = delta_stats.video_bytes_decoded;
  previous_stats_ = current_stats;

  client().statistics_cb.Run(delta_stats);
}

}  // namespace media
}  // namespace chromecast
