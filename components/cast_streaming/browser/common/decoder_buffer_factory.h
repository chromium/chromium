// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_BROWSER_COMMON_DECODER_BUFFER_FACTORY_H_
#define COMPONENTS_CAST_STREAMING_BROWSER_COMMON_DECODER_BUFFER_FACTORY_H_

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"

namespace media {
class DecoderBuffer;
}

namespace openscreen::cast {
struct EncodedFrame;
}

namespace cast_streaming {

// Class used to create media::DecoderBuffer instances from the data provided
// by Openscreen.
class DecoderBufferFactory {
 public:
  virtual ~DecoderBufferFactory() = default;

  // Creates a new DecoderBuffer using the `encoded_frame` and `frame_data`
  // received from Openscreen. On success, this function is expected to
  // return the associated DecoderBuffer. On failure, nullptr is returned.
  //
  // Note: the returned DecoderBuffer does not copy or own the frame data,
  // and its lifetime is tied to the `frame_data` span. Since the frame is
  // synchronously sent over mojo, which performs a copy, this is safe and
  // avoids an extra copy.
  virtual scoped_refptr<media::DecoderBuffer> ToDecoderBuffer(
      const openscreen::cast::EncodedFrame& encoded_frame,
      base::span<const uint8_t> frame_data) = 0;
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_BROWSER_COMMON_DECODER_BUFFER_FACTORY_H_
