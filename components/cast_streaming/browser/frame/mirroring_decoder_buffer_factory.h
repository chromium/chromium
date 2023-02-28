// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_BROWSER_FRAME_MIRRORING_DECODER_BUFFER_FACTORY_H_
#define COMPONENTS_CAST_STREAMING_BROWSER_FRAME_MIRRORING_DECODER_BUFFER_FACTORY_H_

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/cast_streaming/browser/common/decoder_buffer_factory.h"

namespace cast_streaming {

class MirroringDecoderBufferFactory : public DecoderBufferFactory {
 public:
  MirroringDecoderBufferFactory(int receiver_rtp_timebase,
                                base::TimeDelta frame_duration);
  ~MirroringDecoderBufferFactory() override;

  // DecoderBufferFactory implementation.
  scoped_refptr<media::DecoderBuffer> ToDecoderBuffer(
      const openscreen::cast::EncodedFrame& encoded_frame,
      FrameContents& frame_contents) override;

 private:
  // Offset for frames playout time. This is initialized by the first frame.
  base::TimeDelta playout_offset_ = base::TimeDelta::Max();

  const int receiver_rtp_timebase_;
  const base::TimeDelta frame_duration_;
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_BROWSER_FRAME_MIRRORING_DECODER_BUFFER_FACTORY_H_
