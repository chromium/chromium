// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_PROXY_MULTIZONE_AUDIO_DECODER_PROXY_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_PROXY_MULTIZONE_AUDIO_DECODER_PROXY_H_

#include "base/functional/callback.h"
#include "chromecast/media/api/cma_backend.h"
#include "chromecast/media/api/decoder_buffer_base.h"
#include "chromecast/media/cma/backend/proxy/audio_decoder_pipeline_node.h"

namespace chromecast {
namespace media {

// This class is used to decrypt then proxy audio data to an external
// CmaBackend::AudioDecoder over gRPC.
class MultizoneAudioDecoderProxy : public AudioDecoderPipelineNode {
 public:
  MultizoneAudioDecoderProxy(CmaBackend::AudioDecoder* decoder)
      : AudioDecoderPipelineNode(decoder) {}
  ~MultizoneAudioDecoderProxy() override = default;

  virtual void Initialize() = 0;
  virtual void Start(int64_t start_pts) = 0;
  virtual void Stop() = 0;
  virtual void Pause() = 0;
  virtual void Resume() = 0;
  virtual void SetPlaybackRate(float rate) = 0;
  virtual void LogicalPause() = 0;
  virtual void LogicalResume() = 0;
  virtual int64_t GetCurrentPts() const = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_PROXY_MULTIZONE_AUDIO_DECODER_PROXY_H_
