// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <limits>

#include "base/logging.h"
#include "base/stl_util.h"
#include "components/base32/base32.h"
#include "components/base32/base32_test_util.h"

base32::Base32EncodePolicy GetBase32EncodePolicyFromUint8(uint8_t value) {
  // Dummy switch to detect changes to the enum definition.
  switch (base32::Base32EncodePolicy()) {
    case base32::Base32EncodePolicy::INCLUDE_PADDING:
    case base32::Base32EncodePolicy::OMIT_PADDING:
      break;
  }

  return (value % 2) == 0 ? base32::Base32EncodePolicy::INCLUDE_PADDING
                          : base32::Base32EncodePolicy::OMIT_PADDING;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 2 || size > std::numeric_limits<size_t>::max() / 5)
    return 0;

  const base32::Base32EncodePolicy encode_policy =
      GetBase32EncodePolicyFromUint8(data[0]);
  const base::StringPiece string_piece_input(
      reinterpret_cast<const char*>(data + 1), size - 1);
  std::string encoded_string =
      base32::Base32Encode(string_piece_input, encode_policy);
  std::string decoded_string = base32::Base32Decode(encoded_string);

  CHECK_EQ(string_piece_input, decoded_string);
  return 0;
}
