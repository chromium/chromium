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
  // Wrapper around a data buffer used for storing the data of a DecoderBuffer
  // received from Openscreen.
  class FrameContents {
   public:
    virtual ~FrameContents() = default;

    // Returns the span associated with all remaining data for this instance.
    virtual base::span<uint8_t> Get() = 0;

    // Resets the underlying array to size |new_size|.
    virtual bool Reset(uint32_t new_size) = 0;

    // Empties the underlying array.
    virtual void Clear() = 0;

    // Returns the current size of the buffer.
    virtual uint32_t Size() const = 0;

    // Returns whether this instance is empty.
    bool empty() const { return !Size(); }
  };

  virtual ~DecoderBufferFactory() = default;

  // Creates a new DecoderBuffer using the |encoded_frame| and |frame_contents|
  // received from Openscreen. On success, this function is expected to
  // return the associated DecoderBuffer with its contents written to
  // |frame_contents|. On failure, nullptr is returned.
  virtual scoped_refptr<media::DecoderBuffer> ToDecoderBuffer(
      const openscreen::cast::EncodedFrame& encoded_frame,
      FrameContents& frame_contents) = 0;
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_BROWSER_COMMON_DECODER_BUFFER_FACTORY_H_
