// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_COMMON_VIDEO_DECODER_WRAPPER_H_
#define CHROMECAST_MEDIA_COMMON_VIDEO_DECODER_WRAPPER_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "chromecast/media/api/cma_backend.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/media_pipeline_backend.h"

namespace chromecast {
namespace media {
class DecoderBufferBase;

class VideoDecoderWrapper : public CmaBackend::VideoDecoder {
 public:
  // Create a functional VideoDecoderWrapper.
  explicit VideoDecoderWrapper(MediaPipelineBackend::VideoDecoder* decoder);
  // Create a VideoDecoderWrapper that's already been revoked.
  VideoDecoderWrapper();

  VideoDecoderWrapper(const VideoDecoderWrapper&) = delete;
  VideoDecoderWrapper& operator=(const VideoDecoderWrapper&) = delete;

  ~VideoDecoderWrapper() override;

  void Revoke();

 private:
  class RevokedVideoDecoder;

  // CmaBackend::VideoDecoder implementation:
  void SetDelegate(Delegate* delegate) override;
  BufferStatus PushBuffer(scoped_refptr<DecoderBufferBase> buffer) override;
  bool SetConfig(const VideoConfig& config) override;
  void GetStatistics(Statistics* statistics) override;

  scoped_refptr<DecoderBufferBase> pushed_buffer_;

  MediaPipelineBackend::VideoDecoder* decoder_;
  std::unique_ptr<RevokedVideoDecoder> revoked_video_decoder_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_COMMON_VIDEO_DECODER_WRAPPER_H_
