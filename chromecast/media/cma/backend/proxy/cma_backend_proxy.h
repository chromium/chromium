// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_PROXY_CMA_BACKEND_PROXY_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_PROXY_CMA_BACKEND_PROXY_H_

#include <stdint.h>

#include <memory>

#include "base/functional/callback.h"
#include "chromecast/media/api/cma_backend.h"
#include "chromecast/media/cma/backend/proxy/multizone_audio_decoder_proxy.h"

namespace chromecast {
namespace media {

struct MediaPipelineDeviceParams;

// This class is used to intercept audio data and control messages being sent to
// the local CmaBackend and forward it to a remote instance prior to local
// processing in the underlying CmaBackend.
//
// NOTE: By design, this class does NOT handle a/v sync drift between
// |audio_decoder_| and |delegated_pipeline_|.
class CmaBackendProxy : public CmaBackend {
 public:
  using AudioDecoderFactoryCB =
      base::OnceCallback<std::unique_ptr<MultizoneAudioDecoderProxy>()>;

  // Creates a new CmaBackendProxy such that all video processing is delegated
  // to |delegated_video_pipeline|.
  CmaBackendProxy(const MediaPipelineDeviceParams& params,
                  std::unique_ptr<CmaBackend> delegated_video_pipeline);

  CmaBackendProxy(const CmaBackendProxy&) = delete;
  CmaBackendProxy& operator=(const CmaBackendProxy&) = delete;

  ~CmaBackendProxy() override;

  // MediaPipelineBackend implementation:
  //
  // NOTE: Each of these calls must forward the call both to the gRPC Pipe (if
  // the audio decoder exists) and to the local audio decoder (if either audio
  // or video decoding is occuring).
  CmaBackend::AudioDecoder* CreateAudioDecoder() override;
  CmaBackend::VideoDecoder* CreateVideoDecoder() override;
  bool Initialize() override;
  bool Start(int64_t start_pts) override;
  void Stop() override;
  bool Pause() override;
  bool Resume() override;
  int64_t GetCurrentPts() override;
  bool SetPlaybackRate(float rate) override;
  void LogicalPause() override;
  void LogicalResume() override;

 private:
  friend class CmaBackendProxyTest;

  // Creates a new CmaBackendProxy such that all video processing is delegated
  // to |delegated_pipeline| and all audio processing is delegated to a
  // new MultizoneAudioDecoderProxy created by |audio_decoder_factory|.
  CmaBackendProxy(AudioDecoderFactoryCB audio_decoder_factory,
                  std::unique_ptr<CmaBackend> delegated_pipeline);

  // Helper to create the AudioDecoder instance, used with base::bind.
  std::unique_ptr<MultizoneAudioDecoderProxy> CreateAudioDecoderProxy(
      const MediaPipelineDeviceParams& params);

  // The audio decoder to which audio operations should be delegated.
  std::unique_ptr<MultizoneAudioDecoderProxy> audio_decoder_;

  // The CMA Backend to which all video decoding is delegated.
  std::unique_ptr<CmaBackend> delegated_pipeline_;

  // Determines whether a video decoder is being used. If not, calls should not
  // be delegated to the |delegated_pipeline_|, as it may not behave as
  // expected when neither the audio or video decoders are initialized.
  bool has_video_decoder_ = false;

  // The factory to use to populate the |audio_decoder_| object when needed.
  AudioDecoderFactoryCB audio_decoder_factory_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_PROXY_CMA_BACKEND_PROXY_H_
