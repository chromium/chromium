// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_DESKTOP_MEDIA_PIPELINE_BACKEND_DESKTOP_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_DESKTOP_MEDIA_PIPELINE_BACKEND_DESKTOP_H_

#include <stdint.h>

#include <memory>

#include "chromecast/public/media/media_pipeline_backend.h"

namespace chromecast {
namespace media {
class AudioDecoderDesktop;
class VideoDecoderDesktop;

// Factory that instantiates desktop (stub) media pipeline device elements.
class MediaPipelineBackendDesktop : public MediaPipelineBackend {
 public:
  MediaPipelineBackendDesktop();

  MediaPipelineBackendDesktop(const MediaPipelineBackendDesktop&) = delete;
  MediaPipelineBackendDesktop& operator=(const MediaPipelineBackendDesktop&) =
      delete;

  ~MediaPipelineBackendDesktop() override;

  const AudioDecoderDesktop* audio_decoder() const {
    return audio_decoder_.get();
  }
  const VideoDecoderDesktop* video_decoder() const {
    return video_decoder_.get();
  }

  // MediaPipelineBackend implementation:
  AudioDecoder* CreateAudioDecoder() override;
  VideoDecoder* CreateVideoDecoder() override;
  bool Initialize() override;
  bool Start(int64_t start_pts) override;
  void Stop() override;
  bool Pause() override;
  bool Resume() override;
  int64_t GetCurrentPts() override;
  bool SetPlaybackRate(float rate) override;

 private:
  enum State {
    kStateUninitialized,
    kStateInitialized,
    kStatePlaying,
    kStatePaused,
  };
  State state_;
  float rate_;
  std::unique_ptr<AudioDecoderDesktop> audio_decoder_;
  std::unique_ptr<VideoDecoderDesktop> video_decoder_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_DESKTOP_MEDIA_PIPELINE_BACKEND_DESKTOP_H_
