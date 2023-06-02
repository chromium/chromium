// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/allocation_recorder/crash_handler/stream_data_source_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/debug/debugging_buildflags.h"
#include "base/strings/string_piece.h"
#include "components/allocation_recorder/internal/internal.h"
#include "third_party/crashpad/crashpad/minidump/minidump_user_extension_stream_data_source.h"

namespace allocation_recorder::crash_handler {
namespace {

// Wrap the payload into the report begin and end marker, see
// |internal::kReportMarker|.
class WrappedByHeaderAndFooter final
    : public crashpad::MinidumpUserExtensionStreamDataSource {
 public:
  template <typename... ArgTypes>
  WrappedByHeaderAndFooter(ArgTypes&&... payload_args);
  ~WrappedByHeaderAndFooter() override;

  size_t StreamDataSize() override;

  bool ReadStreamData(Delegate* delegate) override;

 private:
  const std::string payload_;
};

template <typename... ArgTypes>
WrappedByHeaderAndFooter::WrappedByHeaderAndFooter(ArgTypes&&... payload_args)
    : crashpad::MinidumpUserExtensionStreamDataSource(
          ::allocation_recorder::internal::kStreamDataType),
      payload_(std::forward<ArgTypes>(payload_args)...) {}

WrappedByHeaderAndFooter::~WrappedByHeaderAndFooter() = default;

size_t WrappedByHeaderAndFooter::StreamDataSize() {
  return 2 * internal::kLengthOfReportMarker + payload_.length();
}

bool WrappedByHeaderAndFooter::ReadStreamData(Delegate* delegate) {
  return delegate->ExtensionStreamDataSourceRead(
             internal::kReportMarker, internal::kLengthOfReportMarker) &&
         delegate->ExtensionStreamDataSourceRead(payload_.c_str(),
                                                 payload_.length()) &&
         delegate->ExtensionStreamDataSourceRead(
             internal::kReportMarker, internal::kLengthOfReportMarker);
}

template <typename... ArgTypes>
std::unique_ptr<WrappedByHeaderAndFooter> MakeWrappedStringStream(
    ArgTypes&&... args) {
  return std::make_unique<WrappedByHeaderAndFooter>(
      std::forward<ArgTypes>(args)...);
}

}  // namespace

StreamDataSourceFactory::~StreamDataSourceFactory() = default;

std::unique_ptr<crashpad::MinidumpUserExtensionStreamDataSource>
StreamDataSourceFactory::CreateErrorMessage(
    base::StringPiece error_message) const {
  return MakeWrappedStringStream(error_message);
}

#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
std::unique_ptr<crashpad::MinidumpUserExtensionStreamDataSource>
StreamDataSourceFactory::CreateReportStream(
    const base::debug::tracer::AllocationTraceRecorder&
        allocation_trace_recorder) const {
  return CreateErrorMessage("!!REPORT CREATION NOT IMPLEMENTED!!");
}
#endif

}  // namespace allocation_recorder::crash_handler
