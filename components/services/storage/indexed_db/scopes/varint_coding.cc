// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/services/storage/indexed_db/scopes/varint_coding.h"

#include "base/check_op.h"

namespace content::indexed_db {

void EncodeVarInt(int64_t from, std::string* into) {
  DCHECK_GE(from, 0);
  // A temporary array is used to amortize the costs of the string modification.
  static constexpr size_t kMaxBytesForUInt64VarInt = 10;
  char temp[kMaxBytesForUInt64VarInt];
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
  into->append(temp, temp_index);
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
    ret |= static_cast<uint64_t>(c & 0x7f) << shift;
    shift += 7;
  } while (*it++ & 0x80);
  *into = static_cast<int64_t>(ret);
  from->remove_prefix(it - from->begin());
  return true;
}

}  // namespace content::indexed_db
