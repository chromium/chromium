// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/base/demuxer_stream_for_test.h"

#include "base/threading/thread.h"
#include "media/base/media_util.h"

namespace chromecast {
namespace media {

DemuxerStreamForTest::DemuxerStreamForTest(int total_frames,
                                           int cycle_count,
                                           int delayed_frame_count,
                                           const std::list<int>& config_idx)
    : total_frame_count_(total_frames),
      cycle_count_(cycle_count),
      delayed_frame_count_(delayed_frame_count),
      config_idx_(config_idx),
      frame_count_(0),
      has_pending_read_(false) {
  DCHECK_LE(delayed_frame_count, cycle_count);
}

DemuxerStreamForTest::~DemuxerStreamForTest() {
}

void DemuxerStreamForTest::Read(ReadCB read_cb) {
  has_pending_read_ = true;
  if (!config_idx_.empty() && config_idx_.front() == frame_count_) {
    config_idx_.pop_front();
    has_pending_read_ = false;
    std::move(read_cb).Run(kConfigChanged,
                           scoped_refptr<::media::DecoderBuffer>());
    return;
  }

  if ((frame_count_ % cycle_count_) < delayed_frame_count_) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DemuxerStreamForTest::DoRead, base::Unretained(this),
                       std::move(read_cb)),
        base::TimeDelta::FromMilliseconds(20));
    return;
  }
  DoRead(std::move(read_cb));
}

::media::AudioDecoderConfig DemuxerStreamForTest::audio_decoder_config() {
  NOTREACHED() << "DemuxerStreamForTest is a video DemuxerStream";
  return ::media::AudioDecoderConfig();
}

::media::VideoDecoderConfig DemuxerStreamForTest::video_decoder_config() {
  gfx::Size coded_size(640, 480);
  gfx::Rect visible_rect(640, 480);
  gfx::Size natural_size(640, 480);
  return ::media::VideoDecoderConfig(
      ::media::kCodecH264, ::media::VIDEO_CODEC_PROFILE_UNKNOWN,
      ::media::VideoDecoderConfig::AlphaMode::kIsOpaque,
      ::media::VideoColorSpace(), ::media::kNoTransformation, coded_size,
      visible_rect, natural_size, ::media::EmptyExtraData(),
      ::media::EncryptionScheme::kUnencrypted);
}

::media::DemuxerStream::Type DemuxerStreamForTest::type() const {
  return VIDEO;
}

bool DemuxerStreamForTest::SupportsConfigChanges() {
  return true;
}

bool DemuxerStreamForTest::IsReadPending() const {
  return has_pending_read_;
}

void DemuxerStreamForTest::DoRead(ReadCB read_cb) {
  has_pending_read_ = false;

  if (total_frame_count_ != -1 && frame_count_ >= total_frame_count_) {
    // End of stream
    std::move(read_cb).Run(kOk, ::media::DecoderBuffer::CreateEOSBuffer());
    return;
  }

  scoped_refptr<::media::DecoderBuffer> buffer(new ::media::DecoderBuffer(16));
  buffer->set_timestamp(frame_count_ * base::TimeDelta::FromMilliseconds(
                                           kDemuxerStreamForTestFrameDuration));
  frame_count_++;
  std::move(read_cb).Run(kOk, buffer);
}

}  // namespace media
}  // namespace chromecast
