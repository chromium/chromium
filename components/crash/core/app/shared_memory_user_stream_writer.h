// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CORE_APP_SHARED_MEMORY_USER_STREAM_WRITER_H_
#define COMPONENTS_CRASH_CORE_APP_SHARED_MEMORY_USER_STREAM_WRITER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>

#include "base/containers/span.h"
#include "base/functional/function_ref.h"
#include "base/memory/read_only_shared_memory_region.h"

namespace crash_reporter {

// Manages writing double-buffered user stream data into shared memory for
// consumption by crash handler processes as minidump user streams.
//
// This class is NOT thread-safe for concurrent writes. If `WriteData()` may be
// called from multiple threads, callers must provide external synchronization.
// However, slot activation updates are atomic, guaranteeing lock-free,
// consistent reads for out-of-process crash handlers.
//
// Updates are double-buffered across two slots (active and inactive):
// - The active slot holds the last consistent snapshot read by crash handlers.
// - The inactive slot is where `WriteData()` writes new updates safely.
// Upon a successful write, the inactive slot is atomically activated.
class SharedMemoryUserStreamWriter {
 public:
  // Constructs a writer, allocates a new shared memory region, and initializes
  // the stream header. `user_stream_type` specifies the Minidump user stream
  // type identifier. `slot_capacity_bytes` specifies the capacity (in bytes) of
  // each data slot (must be non-zero and at most 4 MiB).
  //
  //`CHECK`s on out-of-range parameters, arithmetic overflow, or memory
  // allocation failure.
  SharedMemoryUserStreamWriter(uint32_t user_stream_type,
                               uint32_t slot_capacity_bytes);

  SharedMemoryUserStreamWriter(const SharedMemoryUserStreamWriter&) = delete;
  SharedMemoryUserStreamWriter& operator=(const SharedMemoryUserStreamWriter&) =
      delete;

  // Runs `producer` to generate data into the inactive slot of the shared
  // memory buffer. `producer` receives a writable `base::span<uint8_t>`
  // representing the target slot buffer and should return the number of bytes
  // written, or std::nullopt on failure. Returns true if the write succeeded
  // and the inactive slot became active.
  bool WriteData(
      base::FunctionRef<std::optional<size_t>(base::span<uint8_t>)> producer);

  // Duplicates the underlying read-only region to pass to handler processes.
  base::ReadOnlySharedMemoryRegion DuplicateRegion() const;

 private:
  base::MappedReadOnlyRegion mapped_region_;
  const uint32_t slot_capacity_bytes_;
  uint32_t inactive_slot_index_ = 0;
};

}  // namespace crash_reporter

#endif  // COMPONENTS_CRASH_CORE_APP_SHARED_MEMORY_USER_STREAM_WRITER_H_
