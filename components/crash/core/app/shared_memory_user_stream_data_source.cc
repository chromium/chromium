// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/app/shared_memory_user_stream_data_source.h"

#include <stdint.h>

#include <optional>
#include <vector>

#include "components/crash/core/common/shared_memory_user_stream_reader.h"
#include "third_party/crashpad/crashpad/minidump/minidump_user_extension_stream_data_source.h"

namespace crash_reporter::internal {

namespace {

// Adapter class that wraps a `std::vector<uint8_t>` payload into a Crashpad
// MinidumpUserExtensionStreamDataSource for inclusion in minidumps.
class VectorExtensionStreamDataSource final
    : public crashpad::MinidumpUserExtensionStreamDataSource {
 public:
  explicit VectorExtensionStreamDataSource(UserStreamData&& stream_data)
      : crashpad::MinidumpUserExtensionStreamDataSource(
            stream_data.user_stream_type),
        data_(std::move(stream_data.payload)) {}

  ~VectorExtensionStreamDataSource() override = default;

  size_t StreamDataSize() override { return data_.size(); }

  bool ReadStreamData(Delegate* delegate) override {
    return delegate->ExtensionStreamDataSourceRead(data_.data(), data_.size());
  }

 private:
  const std::vector<uint8_t> data_;
};

}  // namespace

SharedMemoryUserStreamDataSource::SharedMemoryUserStreamDataSource(
    base::ReadOnlySharedMemoryMapping&& mapping)
    : mapping_(std::move(mapping)) {}

SharedMemoryUserStreamDataSource::~SharedMemoryUserStreamDataSource() = default;

std::unique_ptr<crashpad::MinidumpUserExtensionStreamDataSource>
SharedMemoryUserStreamDataSource::ProduceStreamData(
    crashpad::ProcessSnapshot*) {
  if (!mapping_.IsValid()) {
    return nullptr;
  }

  std::optional<UserStreamData> stream_data =
      ExtractSharedMemoryUserStreamData(mapping_);
  if (!stream_data) {
    return nullptr;
  }

  return std::make_unique<VectorExtensionStreamDataSource>(
      std::move(*stream_data));
}

}  // namespace crash_reporter::internal
