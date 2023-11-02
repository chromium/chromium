// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_PIPELINE_AUDIO_PIPELINE_IMPL_H_
#define CHROMECAST_MEDIA_CMA_PIPELINE_AUDIO_PIPELINE_IMPL_H_

#include <memory>
#include <vector>

#include "chromecast/media/api/cma_backend.h"
#include "chromecast/media/cma/pipeline/av_pipeline_client.h"
#include "chromecast/media/cma/pipeline/av_pipeline_impl.h"
#include "chromecast/public/media/stream_id.h"
#include "media/base/pipeline_status.h"

namespace media {
class AudioDecoderConfig;
class VideoDecoderConfig;
}

namespace chromecast {
namespace media {
class CodedFrameProvider;

class AudioPipelineImpl : public AvPipelineImpl {
 public:
  AudioPipelineImpl(CmaBackend::AudioDecoder* decoder, AvPipelineClient client);

  AudioPipelineImpl(const AudioPipelineImpl&) = delete;
  AudioPipelineImpl& operator=(const AudioPipelineImpl&) = delete;

  ~AudioPipelineImpl() override;

  ::media::PipelineStatus Initialize(
      const ::media::AudioDecoderConfig& config,
      std::unique_ptr<CodedFrameProvider> frame_provider);

  void SetVolume(float volume);

  // AvPipelineImpl implementation:
  void UpdateStatistics() override;

 private:
  // AvPipelineImpl implementation:
  void OnUpdateConfig(StreamId id,
                      const ::media::AudioDecoderConfig& audio_config,
                      const ::media::VideoDecoderConfig& video_config) override;
  EncryptionScheme GetEncryptionScheme(StreamId id) const override;
  std::unique_ptr<StreamDecryptor> CreateDecryptor() override;

  CmaBackend::AudioDecoder* const audio_decoder_;

  EncryptionScheme encryption_scheme_ = EncryptionScheme::kUnencrypted;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_PIPELINE_AUDIO_PIPELINE_IMPL_H_
