// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cbor/reader.h"

#include <stdint.h>

#include <optional>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "components/cbor/writer.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

namespace cbor {

DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(const base::span<const uint8_t> input) {
  std::optional<Value> cbor = Reader::Read(input);
  if (cbor.has_value()) {
    std::optional<std::vector<uint8_t>> serialized_cbor =
        Writer::Write(cbor.value());
    CHECK(serialized_cbor.has_value());
    // This can only be reached if the input was canonical, which means that it
    // must exactly match the re-serialized output.
    CHECK(serialized_cbor.value() == input);
  }

  Reader::Config config;
  config.allow_and_canonicalize_out_of_order_keys = true;

  std::optional<Value> cbor_1 = Reader::Read(input, config);
  if (cbor_1.has_value()) {
    std::optional<std::vector<uint8_t>> serialized_cbor =
        Writer::Write(cbor_1.value());
    CHECK(serialized_cbor.has_value());
    // Reordering the keys shouldn't affect the serialized size.
    CHECK(serialized_cbor.value().size() == input.size());
  }

  return 0;
}

}  // namespace cbor
