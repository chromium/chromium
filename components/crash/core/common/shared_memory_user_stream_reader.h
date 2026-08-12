// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CORE_COMMON_SHARED_MEMORY_USER_STREAM_READER_H_
#define COMPONENTS_CRASH_CORE_COMMON_SHARED_MEMORY_USER_STREAM_READER_H_

#include <stdint.h>

#include <optional>
#include <vector>

#include "base/containers/span.h"

namespace crash_reporter {

// Holds minidump user stream metadata and payload extracted from a shared
// memory region.
struct UserStreamData {
  uint32_t user_stream_type;
  std::vector<uint8_t> payload;
};

// Extracts the payload and stream type from `user_stream_buffer`, returning
// std::nullopt if the buffer or active slot is invalid, corrupted, unaligned,
// or modified concurrently during all read attempts.
//
// Even though `user_stream_buffer` is a generic span, it is assumed to point
// to memory formatted according to the SharedMemoryUserStream protocol (defined
// in shared_memory_user_stream.h). It uses retry loops to safely read from
// memory that may be concurrently modified.
std::optional<UserStreamData> ExtractSharedMemoryUserStreamData(
    base::span<const uint8_t> user_stream_buffer);

}  // namespace crash_reporter

#endif  // COMPONENTS_CRASH_CORE_COMMON_SHARED_MEMORY_USER_STREAM_READER_H_
