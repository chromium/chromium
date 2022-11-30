// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/crash_handler/crash_handler.h"

#include <stddef.h>
#include <memory>
#include <string>

#include "base/logging.h"
#include "components/gwp_asan/crash_handler/crash.pb.h"
#include "components/gwp_asan/crash_handler/crash_analyzer.h"
#include "third_party/crashpad/crashpad/minidump/minidump_user_extension_stream_data_source.h"
#include "third_party/crashpad/crashpad/snapshot/process_snapshot.h"

namespace gwp_asan {
namespace internal {
namespace {

// Return a serialized protobuf using a wrapper interface that
// crashpad::UserStreamDataSource expects us to return.
class BufferExtensionStreamDataSource final
    : public crashpad::MinidumpUserExtensionStreamDataSource {
 public:
  BufferExtensionStreamDataSource(uint32_t stream_type, const Crash& crash);

  BufferExtensionStreamDataSource(const BufferExtensionStreamDataSource&) =
      delete;
  BufferExtensionStreamDataSource& operator=(
      const BufferExtensionStreamDataSource&) = delete;

  size_t StreamDataSize() override;
  bool ReadStreamData(Delegate* delegate) override;

 private:
  std::string data_;
};

BufferExtensionStreamDataSource::BufferExtensionStreamDataSource(
    uint32_t stream_type,
    const Crash& crash)
    : crashpad::MinidumpUserExtensionStreamDataSource(stream_type) {
  [[maybe_unused]] bool result = crash.SerializeToString(&data_);
  DCHECK(result);
}

size_t BufferExtensionStreamDataSource::StreamDataSize() {
  DCHECK(!data_.empty());
  return data_.size();
}

bool BufferExtensionStreamDataSource::ReadStreamData(Delegate* delegate) {
  DCHECK(!data_.empty());
  return delegate->ExtensionStreamDataSourceRead(data_.data(), data_.size());
}

const char* ErrorToString(Crash_ErrorType type) {
  switch (type) {
    case Crash::USE_AFTER_FREE:
      return "heap-use-after-free";
    case Crash::BUFFER_UNDERFLOW:
      return "heap-buffer-underflow";
    case Crash::BUFFER_OVERFLOW:
      return "heap-buffer-overflow";
    case Crash::DOUBLE_FREE:
      return "double-free";
    case Crash::UNKNOWN:
      return "unknown";
    case Crash::FREE_INVALID_ADDRESS:
      return "free-invalid-address";
    default:
      return "unexpected error type";
  }
}

const char* AllocatorToString(Crash_Allocator allocator) {
  switch (allocator) {
    case Crash::MALLOC:
      return "malloc";
    case Crash::PARTITIONALLOC:
      return "partitionalloc";
    default:
      return "unexpected allocator type";
  }
}

std::unique_ptr<crashpad::MinidumpUserExtensionStreamDataSource>
HandleException(const crashpad::ProcessSnapshot& snapshot) {
  gwp_asan::Crash proto;
  CrashAnalyzer::GetExceptionInfo(snapshot, &proto);
  // The missing_metadata field is always set for all exceptions.
  if (!proto.has_missing_metadata())
    return nullptr;

  if (proto.missing_metadata()) {
    LOG(ERROR) << "Detected GWP-ASan crash with missing metadata.";
  } else {
    LOG(ERROR) << "Detected GWP-ASan crash for allocation at 0x" << std::hex
               << proto.allocation_address() << std::dec << " ("
               << AllocatorToString(proto.allocator()) << ") of type "
               << ErrorToString(proto.error_type());
  }

  if (proto.has_free_invalid_address()) {
    LOG(ERROR) << "Invalid address passed to free() is " << std::hex
               << proto.free_invalid_address() << std::dec;
  }

  if (proto.has_internal_error())
    LOG(ERROR) << "Experienced internal error: " << proto.internal_error();

  return std::make_unique<BufferExtensionStreamDataSource>(
      kGwpAsanMinidumpStreamType, proto);
}

}  // namespace
}  // namespace internal

std::unique_ptr<crashpad::MinidumpUserExtensionStreamDataSource>
UserStreamDataSource::ProduceStreamData(crashpad::ProcessSnapshot* snapshot) {
  if (!snapshot) {
    DLOG(ERROR) << "Null process snapshot is unexpected.";
    return nullptr;
  }

  return internal::HandleException(*snapshot);
}

}  // namespace gwp_asan
