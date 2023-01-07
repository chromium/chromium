// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_COMMON_PACK_STACK_TRACE_H_
#define COMPONENTS_GWP_ASAN_COMMON_PACK_STACK_TRACE_H_

#include <stddef.h>
#include <stdint.h>

// These routines 'compress' a stack trace by storing a stack trace as a
// starting address, followed by offsets from the previous pointer. All values
// are stored using variable-length integers to reduce space. Furthermore, they
// are zigzag encoded, like in protobuf encoding, to store negative offsets
// efficiently. On 64-bit platforms this packing can reduce space required to
// store a stack trace by over 50%.

namespace gwp_asan {
namespace internal {

// From the stack trace in |unpacked| of length |unpacked_size|, pack it into
// the buffer |packed| with maximum length |packed_max_size|. The return value
// is the number of bytes that were written to the output buffer.
size_t Pack(const uintptr_t* unpacked,
            size_t unpacked_size,
            uint8_t* packed,
            size_t packed_max_size);

// From the packed stack trace in |packed| of length |packed_size|, write the
// unpacked stack trace of maximum length |unpacked_max_size| into |unpacked|.
// Returns the number of entries un packed, or 0 on error.
size_t Unpack(const uint8_t* packed,
              size_t packed_size,
              uintptr_t* unpacked,
              size_t unpacked_max_size);

}  // namespace internal
}  // namespace gwp_asan

#endif  // COMPONENTS_GWP_ASAN_COMMON_PACK_STACK_TRACE_H_
