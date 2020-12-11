// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_PROXY_MULTIZONE_AUDIO_DECODER_PROXY_IMPL_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_PROXY_MULTIZONE_AUDIO_DECODER_PROXY_IMPL_H_

#include <limits>

#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "chromecast/media/api/cma_backend.h"
#include "chromecast/media/api/decoder_buffer_base.h"
#include "chromecast/media/cma/backend/proxy/cma_proxy_handler.h"
#include "chromecast/media/cma/backend/proxy/multizone_audio_decoder_proxy.h"

namespace chromecast {
namespace media {

struct AudioConfig;
struct MediaPipelineDeviceParams;

// This class is used to proxy audio data to an external
// CmaBackend::AudioDecoder over gRPC.
class MultizoneAudioDecoderProxyImpl : public MultizoneAudioDecoderProxy,
                                       public CmaProxyHandler::Client {
 public:
  // Creates a new MultizoneAudioDecoderProxy, such that in the event of an
  // unrecoverable error, |fatal_error_callback| will be called. Fallowing this
  // call, this instance will be in an undefined state.
  MultizoneAudioDecoderProxyImpl(const MediaPipelineDeviceParams& params,
                                 CmaBackend::AudioDecoder* downstream_decoder);
  ~MultizoneAudioDecoderProxyImpl() override;

  // MultizoneAudioDecoderProxy implementation:
  //
  // Note that the methods implementing of CmaBackend::AudioDecoder (which
  // MultizoneAudioDecoderProxy extends) must call both into the downstream
  // decoder and into the |proxy_handler_|, so that audio can be processed both
  // locally and remotely. The remaining methods should NOT call into the
  // downstream CmaBackend, as this is the responsibility of the caller.
  void Initialize() override;
  void Start(int64_t start_pts) override;
  void Stop() override;
  void Pause() override;
  void Resume() override;
  int64_t GetCurrentPts() const override;
  void SetPlaybackRate(float rate) override;
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
  // CmaProxyHandler::Client overrides:
  void OnError() override;
  void OnPipelineStateChange(CmaProxyHandler::PipelineState state) override;
  void OnBytesDecoded(int64_t decoded_byte_count) override;

  // The PTS offset as determined by the receiver of the gRPC endpoint wrapped
  // by this class. This value is updated as new PTS values are received over
  // the IPC.
  int64_t pts_offset_ = std::numeric_limits<int64_t>::min();

  // Number of bytes decoded so far.
  int64_t bytes_decoded_ = 0;

  // Parameters for the Initialize() call captured in the ctor.
  const std::string cast_session_id_;
  const CmaProxyHandler::AudioDecoderOperationMode decoder_mode_;

  // Decoder to which the CmaBackend::AudioDecoder calls should be duplicated
  // (when appropriate). It is expected to be the AudioDecoder associated with
  // the "real" CmaBackend, which plays out audio data using the physical
  // device's hardware. By design, this decoder is always assumed to exist.
  CmaBackend::AudioDecoder* const downstream_decoder_;

  // This is the local instance representing the "remote" backend. All above
  // public method calls should call into this instance to proxy the call to
  // the remote backend.
  std::unique_ptr<CmaProxyHandler> proxy_handler_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_PROXY_MULTIZONE_AUDIO_DECODER_PROXY_IMPL_H_
