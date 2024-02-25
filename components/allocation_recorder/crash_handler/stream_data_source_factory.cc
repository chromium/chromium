// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/allocation_recorder/crash_handler/stream_data_source_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "base/debug/allocation_trace.h"
#include "base/debug/debugging_buildflags.h"
#include "base/strings/stringprintf.h"
#include "components/allocation_recorder/crash_handler/payload.h"
#include "components/allocation_recorder/internal/internal.h"
#include "third_party/crashpad/crashpad/minidump/minidump_user_extension_stream_data_source.h"

namespace allocation_recorder::crash_handler {
namespace {

class StringStreamDataSource final
    : public crashpad::MinidumpUserExtensionStreamDataSource {
 public:
  StringStreamDataSource(std::string payload, uint32_t stream_type);
  ~StringStreamDataSource() override;

  size_t StreamDataSize() override;

  bool ReadStreamData(Delegate* delegate) override;

 private:
  std::string payload_;
};

StringStreamDataSource::StringStreamDataSource(std::string payload,
                                               uint32_t stream_type)
    : crashpad::MinidumpUserExtensionStreamDataSource(stream_type),
      payload_(std::move(payload)) {}

StringStreamDataSource::~StringStreamDataSource() = default;

size_t StringStreamDataSource::StreamDataSize() {
  return std::size(payload_);
}

bool StringStreamDataSource::ReadStreamData(Delegate* delegate) {
  return delegate->ExtensionStreamDataSourceRead(std::data(payload_),
                                                 std::size(payload_));
}

std::unique_ptr<StringStreamDataSource> MakeStringStreamDataSource(
    std::string payload) {
  return std::make_unique<StringStreamDataSource>(
      std::move(payload), ::allocation_recorder::internal::kStreamDataType);
}

bool SerializePayload(const allocation_recorder::Payload& payload,
                      std::string& destination) {
  return payload.SerializeToString(&destination);
}

}  // namespace

StreamDataSourceFactory::~StreamDataSourceFactory() = default;

std::unique_ptr<crashpad::MinidumpUserExtensionStreamDataSource>
StreamDataSourceFactory::CreateErrorMessage(
    std::string_view error_message) const {
  std::string serialized_report;

  if (!SerializePayload(CreatePayloadWithProcessingFailures(error_message),
                        serialized_report)) {
    return MakeStringStreamDataSource(base::StringPrintf(
        "Failed to created error message. Original message was '%s'.",
        std::data(error_message)));
  }

  return MakeStringStreamDataSource(std::move(serialized_report));
}

#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
std::unique_ptr<crashpad::MinidumpUserExtensionStreamDataSource>
StreamDataSourceFactory::CreateReportStream(
    const base::debug::tracer::AllocationTraceRecorder& recorder) const {
  std::string serialized_report;

  if (SerializePayload(CreatePayloadWithMemoryOperationReport(recorder),
                       serialized_report)) {
    return MakeStringStreamDataSource(std::move(serialized_report));
  }

  return CreateErrorMessage("Failed to serialize full report.");
}
#endif

}  // namespace allocation_recorder::crash_handler
