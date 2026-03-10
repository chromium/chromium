// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/containers/span.h"
#include "base/memory/ref_counted.h"
#include "components/media_router/common/providers/cast/channel/cast_channel_enum.h"
#include "components/media_router/common/providers/cast/channel/cast_framer.h"
#include "net/base/io_buffer.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"

namespace cast_channel {

DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(const base::span<const uint8_t> data) {
  if (data.size() > MessageFramer::MessageHeader::max_message_size()) {
    return 0;
  }

  scoped_refptr<net::GrowableIOBuffer> buffer =
      base::MakeRefCounted<net::GrowableIOBuffer>();
  buffer->SetCapacity(MessageFramer::MessageHeader::max_message_size());
  buffer->everything().copy_prefix_from(data);

  std::unique_ptr<MessageFramer> framer =
      std::make_unique<MessageFramer>(buffer.get());
  size_t bytes_ingested = 0u;
  size_t bytes_to_read =
      std::min(framer->BytesRequested(), data.size() - bytes_ingested);
  while (bytes_to_read > 0u && bytes_ingested < data.size()) {
    ChannelError unused_error;
    size_t unused_message_length;
    std::unique_ptr<CastMessage> unused_cast_message =
        framer->Ingest(bytes_to_read, &unused_message_length, &unused_error);
    bytes_ingested += bytes_to_read;
    bytes_to_read =
        std::min(framer->BytesRequested(), data.size() - bytes_ingested);
  }

  return 0;
}

}  // namespace cast_channel
