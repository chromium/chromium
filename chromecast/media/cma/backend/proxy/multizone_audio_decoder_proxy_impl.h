// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_PROXY_MULTIZONE_AUDIO_DECODER_PROXY_IMPL_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_PROXY_MULTIZONE_AUDIO_DECODER_PROXY_IMPL_H_

#include <limits>

#include "base/memory/ref_counted.h"
#include "chromecast/media/api/cma_backend.h"
#include "chromecast/media/api/decoder_buffer_base.h"
#include "chromecast/media/cma/backend/proxy/multizone_audio_decoder_proxy.h"

namespace chromecast {
namespace media {

struct AudioConfig;

// This class is used to proxy audio data to an external
// CmaBackend::AudioDecoder over gRPC.
class MultizoneAudioDecoderProxyImpl : public MultizoneAudioDecoderProxy {
 public:
  // Creates a new MultizoneAudioDecoderProxy, such that in the event of an
  // unrecoverable error, |fatal_error_callback| will be called. Fallowing this
  // call, this instance will be in an undefined state.
  MultizoneAudioDecoderProxyImpl();
  ~MultizoneAudioDecoderProxyImpl() override;

  // MultizoneAudioDecoderProxy implementation:
  bool Initialize() override;
  bool Start(int64_t start_pts) override;
  void Stop() override;
  bool Pause() override;
  bool Resume() override;
  int64_t GetCurrentPts() const override;
  bool SetPlaybackRate(float rate) override;
  void LogicalPause() override;
  void LogicalResume() override;
  void SetDelegate(Delegate* delegate) override;
  BufferStatus PushBuffer(scoped_refptr<DecoderBufferBase> buffer) override;
  bool SetConfig(const AudioConfig& config) override;
  bool SetVolume(float multiplier) override;
  RenderingDelay GetRenderingDelay() override;
  void GetStatistics(Statistics* statistics) override;
  bool RequiresDecryption() override;
  void SetObserver(Observer* observer) override;

 private:
  // The PTS offset as determined by the receiver of the gRPC endpoint wrapped
  // by this class. This value is updated as new PTS values are received over
  // the IPC.
  int64_t pts_offset_ = std::numeric_limits<int64_t>::min();
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_PROXY_MULTIZONE_AUDIO_DECODER_PROXY_IMPL_H_
