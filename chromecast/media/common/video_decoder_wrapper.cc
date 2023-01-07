// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/common/video_decoder_wrapper.h"

#include <utility>

#include "chromecast/media/api/decoder_buffer_base.h"

namespace chromecast {
namespace media {

// This fake VideoDecoder would behave as one with buffer filled
// up and the playback is stalled.
class VideoDecoderWrapper::RevokedVideoDecoder
    : public MediaPipelineBackend::VideoDecoder {
 public:
  explicit RevokedVideoDecoder(const Statistics& statistics)
      : statistics_(statistics) {}

  RevokedVideoDecoder(const RevokedVideoDecoder&) = delete;
  RevokedVideoDecoder& operator=(const RevokedVideoDecoder&) = delete;

  ~RevokedVideoDecoder() override = default;

 private:
  // MediaPipelineBackend::VideoDecoder implementation:
  void SetDelegate(Delegate* delegate) override {}

  BufferStatus PushBuffer(CastDecoderBuffer* buffer) override {
    return MediaPipelineBackend::kBufferPending;
  }

  bool SetConfig(const VideoConfig& config) override { return true; }

  void GetStatistics(Statistics* statistics) override {
    *statistics = statistics_;
  }

  Statistics statistics_;
};

VideoDecoderWrapper::VideoDecoderWrapper(
    MediaPipelineBackend::VideoDecoder* decoder)
    : decoder_(decoder) {
  DCHECK(decoder_);
}

VideoDecoderWrapper::VideoDecoderWrapper() {
  Statistics default_statistics;
  revoked_video_decoder_ =
      std::make_unique<RevokedVideoDecoder>(default_statistics);
  decoder_ = revoked_video_decoder_.get();
}

void VideoDecoderWrapper::Revoke() {
  if (!revoked_video_decoder_) {
    Statistics statistics;
    decoder_->GetStatistics(&statistics);

    revoked_video_decoder_ = std::make_unique<RevokedVideoDecoder>(statistics);
    decoder_ = revoked_video_decoder_.get();
  }
}

VideoDecoderWrapper::~VideoDecoderWrapper() = default;

void VideoDecoderWrapper::SetDelegate(
    media::CmaBackend::VideoDecoder::Delegate* delegate) {
  decoder_->SetDelegate(delegate);
}

media::CmaBackend::BufferStatus VideoDecoderWrapper::PushBuffer(
    scoped_refptr<media::DecoderBufferBase> buffer) {
  pushed_buffer_ = std::move(buffer);
  return decoder_->PushBuffer(pushed_buffer_.get());
}

bool VideoDecoderWrapper::SetConfig(const media::VideoConfig& config) {
  return decoder_->SetConfig(config);
}

void VideoDecoderWrapper::GetStatistics(Statistics* statistics) {
  decoder_->GetStatistics(statistics);
}

}  // namespace media
}  // namespace chromecast
