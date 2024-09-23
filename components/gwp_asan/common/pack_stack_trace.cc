// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/gwp_asan/common/pack_stack_trace.h"

namespace gwp_asan {
namespace internal {

namespace {

// Encode variable-length integer to |out|, returns 0 if there was not enough
// space to finish writing it or the length of the encoded integer otherwise.
size_t VarIntEncode(uintptr_t value, uint8_t* out, size_t out_len) {
  for (size_t i = 0; i < out_len; i++) {
    out[i] = value & 0x7f;
    value >>= 7;
    if (!value)
      return i + 1;

    out[i] |= 0x80;
  }

  return 0;
}

// Decode a variable-length integer to |out|, returns 0 if reading it failed or
// the length of the decoded integer otherwise.
size_t VarIntDecode(const uint8_t* in, size_t in_len, uintptr_t* out) {
  uintptr_t result = 0;
  size_t shift = 0;
  for (size_t i = 0; i < in_len; i++) {
    result |= static_cast<uintptr_t>(in[i] & 0x7f) << shift;
    if (in[i] < 0x80) {
      *out = result;
      return i + 1;
    }

    shift += 7;
    // Disallow overflowing the range of the output integer.
    if (shift >= sizeof(uintptr_t) * 8)
      return 0;
  }

  return 0;
}

uintptr_t ZigzagEncode(uintptr_t value) {
  uintptr_t encoded = value << 1;
  if (static_cast<intptr_t>(value) >= 0)
    return encoded;
  return ~encoded;
}

uintptr_t ZigzagDecode(uintptr_t value) {
  uintptr_t decoded = value >> 1;
  if (!(value & 1))
    return decoded;
  return ~decoded;
}

}  // namespace

size_t Pack(const uintptr_t* unpacked,
            size_t unpacked_size,
            uint8_t* packed,
            size_t packed_max_size) {
  size_t idx = 0;
  for (size_t cur_depth = 0; cur_depth < unpacked_size; cur_depth++) {
    uintptr_t diff = unpacked[cur_depth];
    if (cur_depth > 0)
      diff -= unpacked[cur_depth - 1];
    size_t encoded_len =
        VarIntEncode(ZigzagEncode(diff), packed + idx, packed_max_size - idx);
    if (!encoded_len)
      break;

    idx += encoded_len;
  }

  return idx;
}

size_t Unpack(const uint8_t* packed,
              size_t packed_size,
              uintptr_t* unpacked,
              size_t unpacked_max_size) {
  size_t cur_depth;
  size_t idx = 0;
  for (cur_depth = 0; cur_depth < unpacked_max_size; cur_depth++) {
    uintptr_t encoded_diff;
    size_t decoded_len =
        VarIntDecode(packed + idx, packed_size - idx, &encoded_diff);
    if (!decoded_len)
      break;
    idx += decoded_len;

    unpacked[cur_depth] = ZigzagDecode(encoded_diff);
    if (cur_depth > 0)
      unpacked[cur_depth] += unpacked[cur_depth - 1];
  }

  if (idx != packed_size && cur_depth != unpacked_max_size)
    return 0;

  return cur_depth;
}

}  // namespace internal
}  // namespace gwp_asan
