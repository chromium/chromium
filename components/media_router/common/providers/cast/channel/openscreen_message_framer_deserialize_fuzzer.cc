// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <cstdint>

#include "third_party/openscreen/src/cast/common/channel/message_framer.h"
#include "third_party/openscreen/src/platform/base/span.h"
#include "third_party/protobuf/src/google/protobuf/stubs/logging.h"

// Silence logging from the protobuf library.
google::protobuf::LogSilencer log_silencer;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  openscreen::ByteView buffer(data, size);

  size_t bytes_ingested = 0u;
  while (bytes_ingested < size) {
    openscreen::ErrorOr<
        openscreen::cast::message_serialization::DeserializeResult>
        result =
            openscreen::cast::message_serialization::TryDeserialize(buffer);
    if (result.is_error()) {
      break;
    }
    bytes_ingested += result.value().length;
    buffer = buffer.subspan(result.value().length,
                            buffer.size() - result.value().length);
  }
  return 0;
}
