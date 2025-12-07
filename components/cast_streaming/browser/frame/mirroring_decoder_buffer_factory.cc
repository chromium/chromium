// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/frame/mirroring_decoder_buffer_factory.h"

#include <algorithm>

#include "base/logging.h"
#include "components/cast_streaming/common/public/features.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_util.h"
#include "third_party/openscreen/src/cast/streaming/public/encoded_frame.h"

namespace cast_streaming {

MirroringDecoderBufferFactory::MirroringDecoderBufferFactory(
    int receiver_rtp_timebase,
    base::TimeDelta frame_duration)
    : receiver_rtp_timebase_(receiver_rtp_timebase),
      frame_duration_(frame_duration) {}

MirroringDecoderBufferFactory::~MirroringDecoderBufferFactory() = default;

scoped_refptr<media::DecoderBuffer>
MirroringDecoderBufferFactory::ToDecoderBuffer(
    const openscreen::cast::EncodedFrame& encoded_frame,
    FrameContents& frame_contents) {
  scoped_refptr<media::DecoderBuffer> decoder_buffer =
      base::MakeRefCounted<media::DecoderBuffer>(frame_contents.Size());

  decoder_buffer->set_duration(frame_duration_);
  decoder_buffer->set_is_key_frame(
      encoded_frame.dependency ==
      openscreen::cast::EncodedFrame::Dependency::kKeyFrame);

  base::TimeDelta playout_time = base::Microseconds(
      encoded_frame.rtp_timestamp
          .ToTimeSinceOrigin<std::chrono::microseconds>(receiver_rtp_timebase_)
          .count());

  // Some senders do not send an initial playout time of 0. To work around this,
  // a playout offset is added here. This is NOT done when remoting is enabled
  // because the timestamp of the first frame is used to automatically start
  // playback in such cases.
  if (!IsCastRemotingEnabled()) {
    if (playout_offset_ == base::TimeDelta::Max()) {
      playout_offset_ = playout_time;
    }
    playout_time -= playout_offset_;
  }

  decoder_buffer->set_timestamp(playout_time);

  return decoder_buffer;
}

}  // namespace cast_streaming
