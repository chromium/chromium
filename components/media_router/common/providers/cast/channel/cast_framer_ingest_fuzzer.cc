// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/memory/ref_counted.h"
#include "components/media_router/common/providers/cast/channel/cast_channel_enum.h"
#include "components/media_router/common/providers/cast/channel/cast_framer.h"
#include "net/base/io_buffer.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"

// Silence logging from the protobuf library.
google::protobuf::LogSilencer log_silencer;

namespace cast_channel {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size > MessageFramer::MessageHeader::max_message_size())
    return 0;

  scoped_refptr<net::GrowableIOBuffer> buffer =
      base::MakeRefCounted<net::GrowableIOBuffer>();
  buffer->SetCapacity(MessageFramer::MessageHeader::max_message_size());
  buffer->everything().copy_prefix_from(base::span(data, size));

  std::unique_ptr<MessageFramer> framer =
      std::make_unique<MessageFramer>(buffer.get());
  size_t bytes_ingested = 0u;
  size_t bytes_to_read =
      std::min(framer->BytesRequested(), size - bytes_ingested);
  while (bytes_to_read > 0u && bytes_ingested < size) {
    ChannelError unused_error;
    size_t unused_message_length;
    std::unique_ptr<CastMessage> unused_cast_message =
        framer->Ingest(bytes_to_read, &unused_message_length, &unused_error);
    bytes_ingested += bytes_to_read;
    bytes_to_read = std::min(framer->BytesRequested(), size - bytes_ingested);
  }

  return 0;
}

}  // namespace cast_channel
