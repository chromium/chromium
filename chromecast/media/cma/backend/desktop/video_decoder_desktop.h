// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_DESKTOP_VIDEO_DECODER_DESKTOP_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_DESKTOP_VIDEO_DECODER_DESKTOP_H_

#include <memory>

#include "base/time/time.h"
#include "chromecast/public/media/media_pipeline_backend.h"

namespace chromecast {
namespace media {

class MediaSinkDesktop;

class VideoDecoderDesktop : public MediaPipelineBackend::VideoDecoder {
 public:
  VideoDecoderDesktop();

  VideoDecoderDesktop(const VideoDecoderDesktop&) = delete;
  VideoDecoderDesktop& operator=(const VideoDecoderDesktop&) = delete;

  ~VideoDecoderDesktop() override;

  void Start(base::TimeDelta start_pts);
  void Stop();
  void SetPlaybackRate(float rate);
  base::TimeDelta GetCurrentPts();

  // MediaPipelineBackend::VideoDecoder implementation:
  void SetDelegate(Delegate* delegate) override;
  MediaPipelineBackend::BufferStatus PushBuffer(
      CastDecoderBuffer* buffer) override;
  void GetStatistics(Statistics* statistics) override;
  bool SetConfig(const VideoConfig& config) override;

 private:
  Delegate* delegate_;
  std::unique_ptr<MediaSinkDesktop> sink_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_DESKTOP_VIDEO_DECODER_DESKTOP_H_
