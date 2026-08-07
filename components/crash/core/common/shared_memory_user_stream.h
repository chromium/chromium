// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CORE_COMMON_SHARED_MEMORY_USER_STREAM_H_
#define COMPONENTS_CRASH_CORE_COMMON_SHARED_MEMORY_USER_STREAM_H_

#include <stdint.h>

#include <type_traits>

// This file defines the memory layout for a shared memory region used by an
// application to expose data for minidump user streams to an out-of-process
// crash handler.
//
// A double buffering approach is deliberately chosen for the data payloads. The
// application writes updates to the inactive slot and, upon finishing a
// coherent snapshot, atomically updates `active_slot_header_offset` to point
// to it. This guarantees that if the application crashes mid-update (leaving
// the inactive slot corrupted or incomplete), the previously active slot
// remains intact, providing the crash handler with the last fully consistent
// snapshot.

namespace crash_reporter::internal {

inline constexpr uint32_t kUserStreamMagic =
    0x52545355;  // "USTR" in little-endian
inline constexpr uint32_t kUserStreamVersion = 1;

// Special sentinel offset value indicating that no data slot is currently
// active (i.e. no valid stream data has been written yet).
inline constexpr uint32_t kNoActiveSlotHeaderOffset = 0;

// Main envelope metadata mapping over the shared memory region.
// The two data slots should follow this header in memory.
struct SharedMemoryUserStreamHeader {
  uint32_t magic;
  uint32_t version;

  // Minidump user stream type identifier (e.g. registered with Crashpad).
  uint32_t user_stream_type;

  // Byte offset (relative to the beginning of SharedMemoryUserStreamHeader)
  // of the latest consistent data slot header, or `kNoActiveSlotHeaderOffset`
  // (0) if no data has been written yet. Must be read/written using atomic
  // operations with acquire/release semantics.
  uint32_t active_slot_header_offset;
};
static_assert(std::is_standard_layout_v<SharedMemoryUserStreamHeader> &&
              std::is_trivially_copyable_v<SharedMemoryUserStreamHeader>);

// Header at the beginning of each data slot carrying its generation counter
// and payload size.
struct SharedMemoryUserStreamSlotHeader {
  // Monotonically increasing counter incremented before and after updates.
  // Odd values indicate an in-progress update ("dirty bit" in the low bit).
  // Readers should check this before and after copying a slot to detect
  // concurrent modifications and retry. Must be accessed via acquire/release
  // atomics.
  uint32_t generation;

  // Actual payload size (bytes written) for this data slot.
  uint32_t payload_size;
};
static_assert(std::is_standard_layout_v<SharedMemoryUserStreamSlotHeader> &&
              std::is_trivially_copyable_v<SharedMemoryUserStreamSlotHeader>);

}  // namespace crash_reporter::internal

#endif  // COMPONENTS_CRASH_CORE_COMMON_SHARED_MEMORY_USER_STREAM_H_
