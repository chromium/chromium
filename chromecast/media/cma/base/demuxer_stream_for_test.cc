// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/base/demuxer_stream_for_test.h"

#include "base/task/single_thread_task_runner.h"
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
      frame_count_(0) {
  DCHECK_LE(delayed_frame_count, cycle_count);
}

DemuxerStreamForTest::~DemuxerStreamForTest() {
}

void DemuxerStreamForTest::Read(uint32_t count, ReadCB read_cb) {
  DCHECK_EQ(count, 1u) << "DemuxerStreamForTest only reads a single buffer.";
  if (!config_idx_.empty() && config_idx_.front() == frame_count_) {
    config_idx_.pop_front();
    std::move(read_cb).Run(kConfigChanged, {});
    return;
  }

  if ((frame_count_ % cycle_count_) < delayed_frame_count_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DemuxerStreamForTest::DoRead, base::Unretained(this),
                       std::move(read_cb)),
        base::Milliseconds(20));
    return;
  }
  DoRead(std::move(read_cb));
}

::media::AudioDecoderConfig DemuxerStreamForTest::audio_decoder_config() {
  NOTREACHED() << "DemuxerStreamForTest is a video DemuxerStream";
}

::media::VideoDecoderConfig DemuxerStreamForTest::video_decoder_config() {
  gfx::Size coded_size(640, 480);
  gfx::Rect visible_rect(640, 480);
  gfx::Size natural_size(640, 480);
  return ::media::VideoDecoderConfig(
      ::media::VideoCodec::kH264, ::media::VIDEO_CODEC_PROFILE_UNKNOWN,
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

void DemuxerStreamForTest::DoRead(ReadCB read_cb) {
  if (total_frame_count_ != -1 && frame_count_ >= total_frame_count_) {
    // End of stream
    std::move(read_cb).Run(kOk, {::media::DecoderBuffer::CreateEOSBuffer()});
    return;
  }

  scoped_refptr<::media::DecoderBuffer> buffer(new ::media::DecoderBuffer(16));
  buffer->set_timestamp(frame_count_ *
                        base::Milliseconds(kDemuxerStreamForTestFrameDuration));
  frame_count_++;
  std::move(read_cb).Run(kOk, {std::move(buffer)});
}

}  // namespace media
}  // namespace chromecast
