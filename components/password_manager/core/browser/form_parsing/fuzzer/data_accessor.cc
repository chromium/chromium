// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_parsing/fuzzer/data_accessor.h"

#include <string.h>

#include <algorithm>
#include <bitset>

#include "base/check_op.h"

namespace password_manager {

namespace {

// The maximum byte length of a string to be returned by |ConsumeString*|.
constexpr size_t kMaxStringBytes = 256;

}  // namespace

DataAccessor::DataAccessor(const uint8_t* data, size_t size)
    : data_(data), bits_consumed_(0), size_(size) {
  DCHECK(data_ || size_ == 0);  // Enforce the first invariant for data members.
}

DataAccessor::~DataAccessor() = default;

bool DataAccessor::ConsumeBit() {
  return ConsumeNumber(1) != 0;
}

size_t DataAccessor::ConsumeNumber(size_t bit_length) {
  CHECK_LE(bit_length, sizeof(size_t) * 8);

  // Fast track.
  if (bit_length == 0)
    return 0;

  // No genuine input bits left, return padding.
  if (size_ == 0)
    return 0;

  // Compute the number recursively, processing one byte from |data_| at a time.
  std::bitset<8> b(*data_);
  if (bits_consumed_ + bit_length < 8) {  // Base case: all within |*data_|.
    // Shift the |bit_length|-sized interesting window up and down to discard
    // uninteresting bits. An alternative approach would be:
    // b << bit_length;  // Discard consumed bits.
    // b &= std::bitset<8>((1 << bit_length) - 1); // Discard the tail.
    // But the shifting below avoids the construction of the temproary bitset.
    b <<= (8 - bits_consumed_ - bit_length);
    b >>= (8 - bit_length);
    bits_consumed_ += bit_length;
    return b.to_ulong();
  }
  // Recursive case: crossing the byte boundary in |data_|.
  const size_t original_bits_consumed = bits_consumed_;
  bit_length -= (8 - bits_consumed_);
  bits_consumed_ = 0;
  ++data_;
  --size_;
  return (b.to_ulong() | (ConsumeNumber(bit_length) << 8)) >>
         original_bits_consumed;
}

void DataAccessor::ConsumeBytesToBuffer(size_t length, uint8_t* string_buffer) {
  // First of all, align to a whole byte for efficiency.
  if (size_ > 0 && bits_consumed_ != 0) {
    bits_consumed_ = 0;
    ++data_;
    --size_;
  }

  size_t non_padded_length = std::min(length, size_);
  memcpy(string_buffer, data_, non_padded_length);

  if (non_padded_length != length) {
    // Pad with zeroes as needed.
    memset(string_buffer + non_padded_length, 0, length - non_padded_length);
    // The rest of the input string was not enough, so now it's certainly
    // depleted.
    size_ = 0;
  } else {
    // There was either more of the input string than needed, or just exactly
    // enough bytes of it. Either way, the update below reflects the new
    // situation.
    size_ -= length;
    data_ += length;
  }
}

std::string DataAccessor::ConsumeString(size_t length) {
  CHECK_LE(length, kMaxStringBytes);

  uint8_t string_buffer[kMaxStringBytes];
  ConsumeBytesToBuffer(length, string_buffer);
  return std::string(reinterpret_cast<const char*>(string_buffer), length);
}

std::u16string DataAccessor::ConsumeString16(size_t length) {
  CHECK_LE(2 * length, kMaxStringBytes);

  uint8_t string_buffer[kMaxStringBytes];
  ConsumeBytesToBuffer(2 * length, string_buffer);
  return std::u16string(
      reinterpret_cast<std::u16string::value_type*>(string_buffer), length);
}

}  // namespace password_manager
