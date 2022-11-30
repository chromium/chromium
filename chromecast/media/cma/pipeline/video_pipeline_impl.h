// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_PIPELINE_VIDEO_PIPELINE_IMPL_H_
#define CHROMECAST_MEDIA_CMA_PIPELINE_VIDEO_PIPELINE_IMPL_H_

#include <memory>
#include <vector>

#include "chromecast/media/api/cma_backend.h"
#include "chromecast/media/cma/pipeline/av_pipeline_impl.h"
#include "chromecast/media/cma/pipeline/video_pipeline_client.h"
#include "chromecast/public/media/stream_id.h"
#include "media/base/pipeline_status.h"

namespace media {
class AudioDecoderConfig;
class VideoDecoderConfig;
}

namespace chromecast {
struct Size;
namespace media {
class CodedFrameProvider;

class VideoPipelineImpl : public AvPipelineImpl {
 public:
  VideoPipelineImpl(CmaBackend::VideoDecoder* decoder,
                    VideoPipelineClient client);

  VideoPipelineImpl(const VideoPipelineImpl&) = delete;
  VideoPipelineImpl& operator=(const VideoPipelineImpl&) = delete;

  ~VideoPipelineImpl() override;

  ::media::PipelineStatus Initialize(
      const std::vector<::media::VideoDecoderConfig>& configs,
      std::unique_ptr<CodedFrameProvider> frame_provider);

  // AvPipelineImpl implementation:
  void UpdateStatistics() override;

 private:
  // AvPipelineImpl implementation:
  void OnVideoResolutionChanged(const Size& size) override;
  void OnUpdateConfig(StreamId id,
                      const ::media::AudioDecoderConfig& audio_config,
                      const ::media::VideoDecoderConfig& video_config) override;
  EncryptionScheme GetEncryptionScheme(StreamId id) const override;
  std::unique_ptr<StreamDecryptor> CreateDecryptor() override;

  CmaBackend::VideoDecoder* const video_decoder_;
  const VideoPipelineClient::NaturalSizeChangedCB natural_size_changed_cb_;
  std::vector<EncryptionScheme> encryption_schemes_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_PIPELINE_VIDEO_PIPELINE_IMPL_H_
