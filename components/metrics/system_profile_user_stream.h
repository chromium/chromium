// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_SYSTEM_PROFILE_USER_STREAM_H_
#define COMPONENTS_METRICS_SYSTEM_PROFILE_USER_STREAM_H_

#include <stdint.h>

#include <memory>
#include <string_view>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "components/crash/core/common/shared_memory_user_stream_writer.h"

namespace metrics {

// Manages the SystemProfile minidump user stream in shared memory. Exposes the
// serialized SystemProfile to the crash handler, ensuring that all minidumps
// contain system-profile-metadata regardless of which Chrome process crashed.
class SystemProfileUserStream {
 public:
  static SystemProfileUserStream& Get();

  SystemProfileUserStream(const SystemProfileUserStream&) = delete;
  SystemProfileUserStream& operator=(const SystemProfileUserStream&) = delete;

  // Initializes the internal shared memory region. Embedders prioritizing
  // SystemProfile in their minidumps call this before crash handler
  // spawning. If skipped (e.g., if crash reporting is disabled), calls to
  // `WritePayload` safely no-op.
  void Initialize();

  // Returns a read-only handle to the shared memory region to pass to Crashpad.
  // Returns an invalid region if `Initialize()` has not been called (e.g., if
  // crash reporting is disabled).
  base::ReadOnlySharedMemoryRegion DuplicateSharedMemoryRegion() const;

  // Writes a serialized SystemProfileProto payload into the shared memory
  // buffer. Safely no-ops if `Initialize()` was not called (e.g., if crash
  // reporting is disabled).
  void WritePayload(std::string_view payload);

 private:
  friend class base::NoDestructor<SystemProfileUserStream>;
  friend class SystemProfileUserStreamTest;

  static constexpr uint32_t kSystemProfileSlotCapacityBytes = 32 * 1024;

  SystemProfileUserStream();
  ~SystemProfileUserStream();

  std::unique_ptr<crash_reporter::SharedMemoryUserStreamWriter>
      user_stream_writer_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_SYSTEM_PROFILE_USER_STREAM_H_
