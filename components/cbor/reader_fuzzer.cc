// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <algorithm>

#include "components/cbor/reader.h"  // nogncheck
#include "components/cbor/writer.h"  // nogncheck

namespace cbor {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::vector<uint8_t> input(data, data + size);
  base::Optional<Value> cbor = Reader::Read(input);

  if (cbor.has_value()) {
    base::Optional<std::vector<uint8_t>> serialized_cbor =
        Writer::Write(cbor.value());
    CHECK(serialized_cbor.has_value());
    if (serialized_cbor.has_value()) {
      CHECK(serialized_cbor.value().size() == input.size());
      CHECK(memcmp(serialized_cbor.value().data(), input.data(),
                   input.size()) == 0);
    }
  }
  return 0;
}

}  // namespace cbor
