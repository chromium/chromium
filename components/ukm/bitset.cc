// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/bitset.h"

#include <cstring>

#include "base/check_op.h"
#include "base/containers/span.h"

namespace ukm {

BitSet::BitSet(size_t set_size) : set_size_(set_size) {
  CHECK_GT(set_size_, 0U);
  bitset_.resize(1 + (set_size_ - 1) / 8);
}

BitSet::BitSet(size_t set_size, std::string_view data) : BitSet(set_size) {
  // Copy the passed `data` to the end of the internal `bitset_`. For example,
  // if `data` is {0xAA, 0xBB}, and set_size is 32 (so `bitset_` is a vector of
  // 4 uint8_t's), then the final `bitset_` should be {0x00, 0x00, 0xAA, 0xBB}.
  base::span(bitset_).last(data.size()).copy_from(base::as_byte_span((data)));
}

BitSet::~BitSet() = default;

void BitSet::Add(size_t index) {
  CHECK_LT(index, set_size_);
  size_t internal_index = ToInternalIndex(index);
  uint8_t bitmask = ToBitmask(index);
  bitset_[internal_index] |= bitmask;
}

bool BitSet::Contains(size_t index) const {
  CHECK_LT(index, set_size_);
  size_t internal_index = ToInternalIndex(index);
  uint8_t bitmask = ToBitmask(index);
  return (bitset_[internal_index] & bitmask) != 0;
}

std::string BitSet::Serialize() const {
  // Since the bitset is stored from right to left, as an optimization, omit all
  // the leftmost 0's.
  size_t offset;
  for (offset = 0; offset < bitset_.size(); ++offset) {
    if (bitset_[offset] != 0) {
      break;
    }
  }

  base::span<const char> s =
      base::as_chars(base::span(bitset_).subspan(offset));
  return std::string(s.begin(), s.end());
}

size_t BitSet::ToInternalIndex(size_t index) const {
  // Note: internally, the bitset is stored from right to left. For example,
  // index 0 maps to the least significant bit of the last element of `bitset_`.
  return bitset_.size() - 1 - index / 8;
}

uint8_t BitSet::ToBitmask(size_t index) const {
  return 1 << (index % 8);
}

}  // namespace ukm
