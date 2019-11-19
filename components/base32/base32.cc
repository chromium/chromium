// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/base32/base32.h"

#include <stddef.h>

#include <limits>

#include "base/logging.h"
#include "base/numerics/safe_math.h"

namespace base32 {

std::string Base32Encode(base::StringPiece input, Base32EncodePolicy policy) {
  if (input.empty())
    return std::string();

  // Per RFC4648, the output is formed of 8 characters per 40 bits of input and
  // another 8 characters for the last group of [1,39] bits in the input.
  // That is: ceil(input.size() * 8.0 / 40.0) * 8 ==
  //          ceil(input.size() / 5.0) * 8 ==
  //          ((input.size() + 4) / 5) * 8.
  const size_t padded_length = ((input.size() + 4) / 5) * 8;

  // When no padding is used, the output is exactly 1 character per 5 bits of
  // input and one more for the last [1,4] bits.
  // That is: ceil(input.size() * 8.0 / 5.0) ==
  //          (input.size() * 8 + 4) / 5.
  const size_t unpadded_length =
      ((base::MakeCheckedNum(input.size()) * 8 + 4) / 5).ValueOrDie();

  std::string output;
  const size_t encoded_length = policy == Base32EncodePolicy::INCLUDE_PADDING
                                    ? padded_length
                                    : unpadded_length;
  output.reserve(encoded_length);

  // A bit stream which will be read from the left and appended to from the
  // right as it's emptied.
  uint16_t bit_stream = (static_cast<uint8_t>(input[0]) << 8);
  size_t next_byte_index = 1;
  int free_bits = 8;
  while (free_bits < 16) {
    // Extract the 5 leftmost bits in the stream
    output.push_back(kEncoding[bit_stream >> 11]);
    bit_stream <<= 5;
    free_bits += 5;

    // If there is enough room in the bit stream, inject another byte (if there
    // are any left...).
    if (free_bits >= 8 && next_byte_index < input.size()) {
      free_bits -= 8;
      bit_stream += static_cast<uint8_t>(input[next_byte_index++]) << free_bits;
    }
  }

  if (policy == Base32EncodePolicy::INCLUDE_PADDING) {
    output.append(padded_length - unpadded_length, kPaddingChar);
  }

  DCHECK_EQ(encoded_length, output.size());
  return output;
}

}  // namespace base32
