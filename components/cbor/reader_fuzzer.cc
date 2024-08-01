// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stdint.h>
#include <algorithm>

#include "components/cbor/reader.h"  // nogncheck
#include "components/cbor/writer.h"  // nogncheck

namespace cbor {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::vector<uint8_t> input(data, data + size);

  std::optional<Value> cbor = Reader::Read(input);
  if (cbor.has_value()) {
    std::optional<std::vector<uint8_t>> serialized_cbor =
        Writer::Write(cbor.value());
    CHECK(serialized_cbor.has_value());
    if (serialized_cbor.has_value()) {
      CHECK(serialized_cbor.value().size() == input.size());
      CHECK(memcmp(serialized_cbor.value().data(), input.data(),
                   input.size()) == 0);
    }
  }

  Reader::Config config;
  config.allow_and_canonicalize_out_of_order_keys = true;
  std::optional<Value> cbor_1 = Reader::Read(input, config);

  if (cbor_1.has_value()) {
    std::optional<std::vector<uint8_t>> serialized_cbor =
        Writer::Write(cbor_1.value());
    CHECK(serialized_cbor.has_value());
    if (serialized_cbor.has_value()) {
      CHECK(serialized_cbor.value().size() == input.size());
    }
  }

  return 0;
}

}  // namespace cbor
