// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QR_CODE_GENERATOR_QR_CODE_GENERATOR_H_
#define COMPONENTS_QR_CODE_GENERATOR_QR_CODE_GENERATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/types/expected.h"
#include "components/qr_code_generator/error.h"

namespace qr_code_generator {

// Contains output data from Generate().
// The default state contains no data.
struct GeneratedCode {
 public:
  GeneratedCode();
  GeneratedCode(GeneratedCode&&);
  GeneratedCode& operator=(GeneratedCode&&);

  GeneratedCode(const GeneratedCode&) = delete;
  GeneratedCode& operator=(const GeneratedCode&) = delete;

  ~GeneratedCode();

  // Pixel data.  The least-significant bit of each byte is set if that
  // tile/module should be "black".
  //
  // Clients should ensure four tiles/modules of padding when rendering the
  // code.
  //
  // On error, will not be populated, and will contain an empty vector.
  std::vector<uint8_t> data;

  // Width and height (which are equal) of the generated data, in
  // tiles/modules.
  //
  // The following invariant holds: `qr_size * qr_size == data.size()`.
  //
  // On error, will not be populated, and will contain 0.
  int qr_size = 0;
};

// Generates a QR code containing the given data.
// The generator will attempt to choose a version that fits the data and which
// is >= |min_version|, if given. The returned span's length is
// input-dependent and not known at compile-time.
base::expected<GeneratedCode, Error> GenerateCode(
    base::span<const uint8_t> in,
    std::optional<int> min_version = std::nullopt);

}  // namespace qr_code_generator

#endif  // COMPONENTS_QR_CODE_GENERATOR_QR_CODE_GENERATOR_H_
