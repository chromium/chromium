// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/base32/base32.h"

#include <stddef.h>

#include <array>
#include <limits>
#include <string_view>

#include "base/check_op.h"
#include "base/numerics/safe_math.h"

namespace base32 {

namespace {

constexpr auto kEncoding =
    std::to_array<const char>("ABCDEFGHIJKLMNOPQRSTUVWXYZ234567");
static_assert(kEncoding.size() == 33);  // 32 symbols + null terminator
constexpr char kPaddingChar = '=';

// Returns a 5 bit number between [0,31] matching the provided base 32 encoded
// character. Returns 0xff on error.
uint8_t ReverseMapping(char input_char) {
  if (input_char >= 'A' && input_char <= 'Z')
    return input_char - 'A';
  if (input_char >= '2' && input_char <= '7')
    return input_char - '2' + 26;
  return 0xff;
}

}  // namespace

std::string Base32Encode(base::span<const uint8_t> input,
                         Base32EncodePolicy policy) {
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

std::vector<uint8_t> Base32Decode(std::string_view input) {
  // Remove padding, if any
  const size_t padding_index = input.find(kPaddingChar);
  if (padding_index != std::string_view::npos) {
    input.remove_suffix(input.size() - padding_index);
  }

  if (input.empty())
    return std::vector<uint8_t>();

  const size_t decoded_length =
      (base::MakeCheckedNum(input.size()) * 5 / 8).ValueOrDie();

  std::vector<uint8_t> output;
  output.reserve(decoded_length);

  // A bit stream which will be read from the left and appended to from the
  // right as it's emptied.
  uint16_t bit_stream = 0;
  size_t free_bits = 16;
  for (char input_char : input) {
    const uint8_t decoded_5bits = ReverseMapping(input_char);
    // If an invalid character is read from the input, then stop decoding.
    if (decoded_5bits >= 32)
      return std::vector<uint8_t>();

    // Place the next decoded 5-bits in the stream.
    bit_stream |= decoded_5bits << (free_bits - 5);
    free_bits -= 5;

    // If the stream is filled with a byte, flush the stream of that byte and
    // append it to the output.
    if (free_bits <= 8) {
      output.push_back(static_cast<uint8_t>(bit_stream >> 8));
      bit_stream <<= 8;
      free_bits += 8;
    }
  }

  DCHECK_EQ(decoded_length, output.size());
  return output;
}

}  // namespace base32
