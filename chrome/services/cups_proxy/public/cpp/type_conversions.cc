// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_proxy/public/cpp/type_conversions.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/ranges/algorithm.h"

namespace ipp_converter {

// Implicit conversion is safe since the conversion preserves memory layout.
std::vector<uint8_t> ConvertToByteBuffer(std::string_view char_buffer) {
  std::vector<uint8_t> byte_buffer;
  byte_buffer.resize(char_buffer.size());

  base::ranges::copy(char_buffer, byte_buffer.begin());
  return byte_buffer;
}

}  // namespace ipp_converter
