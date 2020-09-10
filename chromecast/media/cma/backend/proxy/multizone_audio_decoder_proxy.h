// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_PROXY_MULTIZONE_AUDIO_DECODER_PROXY_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_PROXY_MULTIZONE_AUDIO_DECODER_PROXY_H_

#include "base/memory/ref_counted.h"
#include "chromecast/media/api/cma_backend.h"
#include "chromecast/media/api/decoder_buffer_base.h"

namespace chromecast {
namespace media {

struct AudioConfig;

// This class is used to decrypt then proxy audio data to an external
// CmaBackend::AudioDecoder over gRPC.
class MultizoneAudioDecoderProxy : public CmaBackend::AudioDecoder {
 public:
  using BufferStatus = CmaBackend::Decoder::BufferStatus;
  using Delegate = CmaBackend::Decoder::Delegate;
  using Observer = CmaBackend::AudioDecoder::Observer;
  using RenderingDelay = CmaBackend::AudioDecoder::RenderingDelay;
  using Statistics = CmaBackend::AudioDecoder::Statistics;

  ~MultizoneAudioDecoderProxy() override;

  // CmaBackend::AudioDecoder implementation:
  void SetDelegate(Delegate* delegate) override;
  BufferStatus PushBuffer(scoped_refptr<DecoderBufferBase> buffer) override;
  bool SetConfig(const AudioConfig& config) override;
  bool SetVolume(float multiplier) override;
  RenderingDelay GetRenderingDelay() override;
  void GetStatistics(Statistics* statistics) override;
  bool RequiresDecryption() override;
  void SetObserver(Observer* observer) override;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_PROXY_MULTIZONE_AUDIO_DECODER_PROXY_H_
