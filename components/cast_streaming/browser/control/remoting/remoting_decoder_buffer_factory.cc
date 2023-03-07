// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/control/remoting/remoting_decoder_buffer_factory.h"

#include <algorithm>

#include "base/logging.h"
#include "media/base/decoder_buffer.h"
#include "media/cast/openscreen/remoting_proto_utils.h"
#include "third_party/openscreen/src/cast/streaming/encoded_frame.h"

namespace cast_streaming {

RemotingDecoderBufferFactory::RemotingDecoderBufferFactory() = default;

RemotingDecoderBufferFactory::~RemotingDecoderBufferFactory() = default;

scoped_refptr<media::DecoderBuffer>
RemotingDecoderBufferFactory::ToDecoderBuffer(
    const openscreen::cast::EncodedFrame& encoded_frame,
    FrameContents& frame_contents) {
  auto span = frame_contents.Get();
  scoped_refptr<media::DecoderBuffer> decoder_buffer =
      media::cast::ByteArrayToDecoderBuffer(span.data(), span.size());
  if (!decoder_buffer) {
    DLOG(WARNING) << "Deserialization failed!";
    return nullptr;
  }

  if (!frame_contents.Reset(decoder_buffer->data_size())) {
    DLOG(WARNING) << "Buffer overflow!";
    return nullptr;
  }

  // Replace the old contents of |frame_contents| (the entire Decoderbuffer)
  // with just the |DecoderBuffer::data()| field per method contract.
  span = frame_contents.Get();
  base::span<const uint8_t> decoder_buffer_data(decoder_buffer->data(),
                                                decoder_buffer->data_size());
  std::copy(decoder_buffer_data.begin(), decoder_buffer_data.end(),
            span.begin());

  return decoder_buffer;
}

}  // namespace cast_streaming
