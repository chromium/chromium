// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CORE_APP_SHARED_MEMORY_USER_STREAM_DATA_SOURCE_H_
#define COMPONENTS_CRASH_CORE_APP_SHARED_MEMORY_USER_STREAM_DATA_SOURCE_H_

#include <memory>

#include "base/memory/shared_memory_mapping.h"
#include "third_party/crashpad/crashpad/handler/user_stream_data_source.h"

namespace crash_reporter::internal {

// A crashpad::UserStreamDataSource that extracts minidump user stream data from
// a shared memory mapping conforming to the SharedMemoryUserStream protocol.
class SharedMemoryUserStreamDataSource : public crashpad::UserStreamDataSource {
 public:
  // `mapping` must contain structured SharedMemoryUserStream data.
  explicit SharedMemoryUserStreamDataSource(
      base::ReadOnlySharedMemoryMapping&& mapping);

  SharedMemoryUserStreamDataSource(const SharedMemoryUserStreamDataSource&) =
      delete;
  SharedMemoryUserStreamDataSource& operator=(
      const SharedMemoryUserStreamDataSource&) = delete;

  ~SharedMemoryUserStreamDataSource() override;

  // Reads the active snapshot from `mapping_` at the time of the crash. Returns
  // a data source containing the extracted stream on success, or `nullptr` if
  // the mapping is invalid, corrupted, or contains no active data.
  std::unique_ptr<crashpad::MinidumpUserExtensionStreamDataSource>
  ProduceStreamData(crashpad::ProcessSnapshot* process_snapshot) override;

 private:
  base::ReadOnlySharedMemoryMapping mapping_;
};

}  // namespace crash_reporter::internal

#endif  // COMPONENTS_CRASH_CORE_APP_SHARED_MEMORY_USER_STREAM_DATA_SOURCE_H_
