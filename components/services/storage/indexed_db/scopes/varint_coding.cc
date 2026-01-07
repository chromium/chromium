// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/scopes/varint_coding.h"

#include <array>

#include "base/check_op.h"

namespace content::indexed_db {

void EncodeVarInt(int64_t from, std::string* into) {
  DCHECK_GE(from, 0);
  // A temporary array is used to amortize the costs of the string modification.
  static constexpr size_t kMaxBytesForUInt64VarInt = 10;
  std::array<char, kMaxBytesForUInt64VarInt> temp;
  uint64_t n = static_cast<uint64_t>(from);
  size_t temp_index = 0;
  do {
    unsigned char c = n & 0x7f;
    n >>= 7;
    if (n)
      c |= 0x80;
    DCHECK_LT(temp_index, kMaxBytesForUInt64VarInt);
    temp[temp_index] = c;
    ++temp_index;
  } while (n);
  into->append(temp.data(), temp_index);
}

bool DecodeVarInt(std::string_view* from, int64_t* into) {
  std::string_view::const_iterator it = from->begin();
  int shift = 0;
  uint64_t ret = 0;
  do {
    // Shifting 64 or more bits is undefined behavior.
    if (it == from->end() || shift >= 64)
      return false;

    unsigned char c = *it;

    if ((shift != 0) && (c == 0)) {
      // On the first iteration, the entire byte can be 0, which represents the
      // value 0. On every other iteration (input byte), this is not a valid
      // input, as EncodeVarInt() would have marked the top bit of the previous
      // byte as 0, and iteration would have stopped.
      return false;
    }

    uint64_t pre_shift = static_cast<uint64_t>(c & 0x7f);
    uint64_t shifted = pre_shift << shift;
    if ((shifted >> shift) != pre_shift) {
      // Make sure that no bits are shifted off the left, which would be another
      // form of invalid input (which can occur in the last byte).
      return false;
    }

    ret |= shifted;
    shift += 7;
  } while (*it++ & 0x80);

  *into = static_cast<int64_t>(ret);
  from->remove_prefix(it - from->begin());
  return true;
}

}  // namespace content::indexed_db
