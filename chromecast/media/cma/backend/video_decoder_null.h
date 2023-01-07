// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_VIDEO_DECODER_NULL_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_VIDEO_DECODER_NULL_H_

#include <stdint.h>

#include "base/memory/weak_ptr.h"
#include "chromecast/media/cma/backend/video_decoder_for_mixer.h"

namespace chromecast {
namespace media {

class VideoDecoderNull : public VideoDecoderForMixer {
 public:
  VideoDecoderNull();

  VideoDecoderNull(const VideoDecoderNull&) = delete;
  VideoDecoderNull& operator=(const VideoDecoderNull&) = delete;

  ~VideoDecoderNull() override;

  // MediaPipelineBackend::VideoDecoder implementation:
  void SetDelegate(Delegate* delegate) override;
  MediaPipelineBackend::BufferStatus PushBuffer(
      CastDecoderBuffer* buffer) override;
  void GetStatistics(Statistics* statistics) override;
  bool SetConfig(const VideoConfig& config) override;

  bool Initialize() override;
  void SetObserver(VideoDecoderForMixer::Observer* observer) override;
  bool Start(int64_t start_pts, bool need_avsync) override;
  void Stop() override;
  bool Pause() override;
  bool Resume() override;
  bool GetCurrentPts(int64_t* timestamp, int64_t* pts) const override;
  bool SetPlaybackRate(float rate) override;
  bool SetPts(int64_t timestamp, int64_t pts) override;
  int64_t GetDroppedFrames() override;
  int64_t GetRepeatedFrames() override;
  int64_t GetOutputRefreshRate() override;
  int64_t GetCurrentContentRefreshRate() override;

 private:
  void OnEndOfStream();

  Delegate* delegate_;
  Observer* observer_;
  base::WeakPtrFactory<VideoDecoderNull> weak_factory_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_VIDEO_DECODER_NULL_H_
