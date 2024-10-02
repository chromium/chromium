// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ALLOCATION_RECORDER_CRASH_HANDLER_PAYLOAD_H_
#define COMPONENTS_ALLOCATION_RECORDER_CRASH_HANDLER_PAYLOAD_H_

#include <string_view>

#include "base/containers/span.h"
#include "components/allocation_recorder/crash_handler/memory_operation_report.pb.h"

namespace base::debug::tracer {
class AllocationTraceRecorder;
}  // namespace base::debug::tracer

namespace allocation_recorder::crash_handler {

// Various functions to create the payload that is included into to the Crashpad
// report. The payload is a Protocol Buffers message. Please see
// memory_operation_report.proto for the exact content and format.

// Create a payload with the MemoryOperationReport set from the content of the
// passed |recorder|.
allocation_recorder::Payload CreatePayloadWithMemoryOperationReport(
    const base::debug::tracer::AllocationTraceRecorder& recorder);

// Create a payload with the ProcessingFailures set from the passed
// |error_messages|.
allocation_recorder::Payload CreatePayloadWithProcessingFailures(
    base::span<const std::string_view> error_messages);

// Create a payload with the ProcessingFailures set from the passed
// |error_message|.
inline allocation_recorder::Payload CreatePayloadWithProcessingFailures(
    std::string_view error_message) {
  return CreatePayloadWithProcessingFailures(
      base::make_span(&error_message, 1ul));
}

// Create a payload with the ProcessingFailures set from the passed
// |error_messages|.
template <size_t Extent>
inline allocation_recorder::Payload CreatePayloadWithProcessingFailures(
    base::span<const std::string_view, Extent> error_messages) {
  base::span<const std::string_view, base::dynamic_extent>
      error_messages_as_dynamic_span =
          base::make_span(std::begin(error_messages), Extent);

  return CreatePayloadWithProcessingFailures(error_messages_as_dynamic_span);
}

}  // namespace allocation_recorder::crash_handler
#endif  // COMPONENTS_ALLOCATION_RECORDER_CRASH_HANDLER_PAYLOAD_H_
