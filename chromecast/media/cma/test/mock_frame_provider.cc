// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/test/mock_frame_provider.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromecast/media/api/decoder_buffer_base.h"
#include "chromecast/media/cma/test/frame_generator_for_test.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_util.h"
#include "media/base/video_decoder_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace chromecast {
namespace media {

MockFrameProvider::MockFrameProvider() : delay_flush_(false) {
}

MockFrameProvider::~MockFrameProvider() {
}

void MockFrameProvider::Configure(
    const std::vector<bool>& delayed_task_pattern,
    std::unique_ptr<FrameGeneratorForTest> frame_generator) {
  delayed_task_pattern_ = delayed_task_pattern;
  pattern_idx_ = 0;

  frame_generator_ = std::move(frame_generator);
}

void MockFrameProvider::SetDelayFlush(bool delay_flush) {
  delay_flush_ = delay_flush;
}

void MockFrameProvider::Read(ReadCB read_cb) {
  bool delayed = delayed_task_pattern_[pattern_idx_];
  pattern_idx_ = (pattern_idx_ + 1) % delayed_task_pattern_.size();

  if (delayed) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&MockFrameProvider::DoRead, base::Unretained(this),
                       std::move(read_cb)),
        base::Milliseconds(1));
  } else {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&MockFrameProvider::DoRead,
                                  base::Unretained(this), std::move(read_cb)));
  }
}

void MockFrameProvider::Flush(base::OnceClosure flush_cb) {
  if (delay_flush_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, std::move(flush_cb), base::Milliseconds(10));
  } else {
    std::move(flush_cb).Run();
  }
}

void MockFrameProvider::DoRead(ReadCB read_cb) {
  bool has_config = frame_generator_->HasDecoderConfig();

  scoped_refptr<DecoderBufferBase> buffer(frame_generator_->Generate());
  ASSERT_TRUE(buffer.get());

  ::media::AudioDecoderConfig audio_config;
  ::media::VideoDecoderConfig video_config;
  if (has_config) {
    gfx::Size coded_size(640, 480);
    gfx::Rect visible_rect(640, 480);
    gfx::Size natural_size(640, 480);
    video_config = ::media::VideoDecoderConfig(
        ::media::VideoCodec::kH264, ::media::VIDEO_CODEC_PROFILE_UNKNOWN,
        ::media::VideoDecoderConfig::AlphaMode::kIsOpaque,
        ::media::VideoColorSpace(), ::media::kNoTransformation, coded_size,
        visible_rect, natural_size, ::media::EmptyExtraData(),
        ::media::EncryptionScheme::kUnencrypted);

    audio_config = ::media::AudioDecoderConfig(
        ::media::AudioCodec::kAAC, ::media::kSampleFormatS16,
        ::media::CHANNEL_LAYOUT_STEREO, 44100, ::media::EmptyExtraData(),
        ::media::EncryptionScheme::kUnencrypted);
  }

  std::move(read_cb).Run(buffer, audio_config, video_config);
}

}  // namespace media
}  // namespace chromecast
