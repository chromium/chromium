// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/base32/base32_test_util.h"

#include <stddef.h>

#include <limits>

#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "components/base32/base32.h"

namespace base32 {

namespace {

// Returns a 5 bit number between [0,31] matching the provided base 32 encoded
// character. Returns 0xff on error.
uint8_t ReverseMapping(char input_char) {
  if (input_char >= 'A' && input_char <= 'Z')
    return input_char - 'A';
  else if (input_char >= '2' && input_char <= '7')
    return input_char - '2' + 26;

  NOTREACHED() << "Invalid base32 character";
  return 0xff;
}

}  // namespace

std::string Base32Decode(base::StringPiece input) {
  if (input.empty())
    return std::string();

  // Remove padding, if any
  const size_t padding_index = input.find(kPaddingChar);
  if (padding_index != base::StringPiece::npos)
    input.remove_suffix(input.size() - padding_index);

  const size_t decoded_length =
      (base::MakeCheckedNum(input.size()) * 5 / 8).ValueOrDie();

  std::string output;
  output.reserve(decoded_length);

  // A bit stream which will be read from the left and appended to from the
  // right as it's emptied.
  uint16_t bit_stream = 0;
  size_t free_bits = 16;
  for (char input_char : input) {
    const uint8_t decoded_5bits = ReverseMapping(input_char);
    // If an invalid character is read from the input, then stop decoding.
    if (decoded_5bits >= 32)
      return std::string();

    // Place the next decoded 5-bits in the stream.
    bit_stream |= decoded_5bits << (free_bits - 5);
    free_bits -= 5;

    // If the stream is filled with a byte, flush the stream of that byte and
    // append it to the output.
    if (free_bits <= 8) {
      output.push_back(static_cast<char>(bit_stream >> 8));
      bit_stream <<= 8;
      free_bits += 8;
    }
  }

  DCHECK_EQ(decoded_length, output.size());
  return output;
}

}  // namespace base32
