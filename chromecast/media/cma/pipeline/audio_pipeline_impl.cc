// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/pipeline/audio_pipeline_impl.h"

#include <stddef.h>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "chromecast/media/cma/base/buffering_defs.h"
#include "chromecast/media/cma/base/coded_frame_provider.h"
#include "chromecast/media/cma/base/decoder_config_adapter.h"
#include "chromecast/media/cma/pipeline/backend_decryptor.h"
#include "chromecast/media/cma/pipeline/cdm_decryptor.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/media_capabilities_shlib.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "media/base/audio_decoder_config.h"

namespace chromecast {
namespace media {

namespace {
const size_t kMaxAudioFrameSize = 32 * 1024;

}

AudioPipelineImpl::AudioPipelineImpl(CmaBackend::AudioDecoder* decoder,
                                     AvPipelineClient client)
    : AvPipelineImpl(decoder, std::move(client)), audio_decoder_(decoder) {
  DCHECK(audio_decoder_);
}

AudioPipelineImpl::~AudioPipelineImpl() = default;

::media::PipelineStatus AudioPipelineImpl::Initialize(
    const ::media::AudioDecoderConfig& audio_config,
    std::unique_ptr<CodedFrameProvider> frame_provider) {
  LOG(INFO) << __FUNCTION__ << " " << audio_config.AsHumanReadableString();
  if (frame_provider) {
    SetCodedFrameProvider(std::move(frame_provider), kAppAudioBufferSize,
                          kMaxAudioFrameSize);
  }

  DCHECK(audio_config.IsValidConfig());
  AudioConfig config =
      DecoderConfigAdapter::ToCastAudioConfig(kPrimary, audio_config);
  encryption_scheme_ = config.encryption_scheme;
  config.encryption_scheme = EncryptionScheme::kUnencrypted;

  if (!audio_decoder_->SetConfig(config)) {
    return ::media::PIPELINE_ERROR_INITIALIZATION_FAILED;
  }
  set_state(kFlushed);
  return ::media::PIPELINE_OK;
}

void AudioPipelineImpl::SetVolume(float volume) {
  audio_decoder_->SetVolume(volume);
}

void AudioPipelineImpl::OnUpdateConfig(
    StreamId id,
    const ::media::AudioDecoderConfig& audio_config,
    const ::media::VideoDecoderConfig& video_config) {
  if (audio_config.IsValidConfig()) {
    LOG(INFO) << __FUNCTION__ << " id:" << id << " "
              << audio_config.AsHumanReadableString();

    AudioConfig config =
        DecoderConfigAdapter::ToCastAudioConfig(id, audio_config);
    encryption_scheme_ = config.encryption_scheme;
    config.encryption_scheme = EncryptionScheme::kUnencrypted;
    bool success = audio_decoder_->SetConfig(config);
    if (!success && !client().playback_error_cb.is_null())
      client().playback_error_cb.Run(::media::PIPELINE_ERROR_DECODE);
  }
}

EncryptionScheme AudioPipelineImpl::GetEncryptionScheme(StreamId id) const {
  return encryption_scheme_;
}

std::unique_ptr<StreamDecryptor> AudioPipelineImpl::CreateDecryptor() {
  if (MediaPipelineBackend::CreateAudioDecryptor) {
    DCHECK_NE(encryption_scheme_, EncryptionScheme::kUnencrypted);
    LOG(INFO) << __func__ << " Create backend decryptor for audio.";
    return std::make_unique<BackendDecryptor>(encryption_scheme_);
  }

  return std::make_unique<CdmDecryptor>(true /* clear_buffer_needed */);
}

void AudioPipelineImpl::UpdateStatistics() {
  if (client().statistics_cb.is_null())
    return;

  // TODO(mbjorge): Give Statistics a default constructor when the
  // next system update happens. b/32802298
  CmaBackend::AudioDecoder::Statistics audio_stats = {};
  audio_decoder_->GetStatistics(&audio_stats);

  ::media::PipelineStatistics current_stats;
  current_stats.audio_bytes_decoded = audio_stats.decoded_bytes;

  ::media::PipelineStatistics delta_stats;
  delta_stats.audio_bytes_decoded =
      current_stats.audio_bytes_decoded - previous_stats_.audio_bytes_decoded;

  bytes_decoded_since_last_update_ = delta_stats.audio_bytes_decoded;
  previous_stats_ = current_stats;

  client().statistics_cb.Run(delta_stats);
}

}  // namespace media
}  // namespace chromecast
