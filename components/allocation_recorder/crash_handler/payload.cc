// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/allocation_recorder/crash_handler/payload.h"

#include <iterator>
#include <string>

#include "base/debug/allocation_trace.h"
#include "base/debug/debugging_buildflags.h"
#include "base/strings/stringprintf.h"
#include "components/allocation_recorder/crash_handler/memory_operation_report.pb.h"

namespace allocation_recorder::crash_handler {
namespace {

::allocation_recorder::Statistics ConvertRecorderStatistics(
    const base::debug::tracer::AllocationTraceRecorderStatistics&
        recorder_statistics) {
  ::allocation_recorder::Statistics data;

  data.set_total_number_of_operations(
      recorder_statistics.total_number_of_allocations);

#if BUILDFLAG(ENABLE_ALLOCATION_TRACE_RECORDER_FULL_REPORTING)
  data.set_total_number_of_collisions(
      recorder_statistics.total_number_of_collisions);
#endif

  return data;
}

::allocation_recorder::StackFrame ConvertStackFrame(const void* const frame) {
  ::allocation_recorder::StackFrame converted_frame;

  converted_frame.set_address(reinterpret_cast<uint64_t>(frame));

  return converted_frame;
}

::allocation_recorder::OperationType ConvertOperationType(
    base::debug::tracer::OperationType operation_type) {
  switch (operation_type) {
    case base::debug::tracer::OperationType::kNone:
      return allocation_recorder::OperationType::NONE;
    case base::debug::tracer::OperationType::kAllocation:
      return allocation_recorder::OperationType::ALLOCATION;
    case base::debug::tracer::OperationType::kFree:
      return allocation_recorder::OperationType::FREE;
  }
}

::allocation_recorder::StackTrace ConvertCallStack(
    const base::debug::tracer::StackTraceContainer& source_trace) {
  // We try to copy an _interesting_ section of the original stack trace. For
  // this we find the first null entry (aka the top of the stack) and copy the
  // n frames before it.
  const auto first_null_entry =
      std::find(std::begin(source_trace), std::end(source_trace), nullptr);

  ::allocation_recorder::StackTrace data;

  for (auto current_item = std::begin(source_trace);
       current_item != first_null_entry; ++current_item) {
    data.mutable_frames()->Add(ConvertStackFrame(*current_item));
  }

  return data;
}

allocation_recorder::MemoryOperation ConvertSingleRecord(
    const base::debug::tracer::OperationRecord& src_record) {
  allocation_recorder::MemoryOperation data;

  data.set_operation_type(ConvertOperationType(src_record.GetOperationType()));
  data.set_address(reinterpret_cast<uint64_t>(src_record.GetAddress()));
  if (src_record.GetOperationType() ==
      base::debug::tracer::OperationType::kAllocation) {
    data.set_size(src_record.GetSize());
  }
  *(data.mutable_stack_trace()) = ConvertCallStack(src_record.GetStackTrace());

  return data;
}

google::protobuf::RepeatedPtrField<allocation_recorder::MemoryOperation>
ConvertMemoryOperations(
    const base::debug::tracer::AllocationTraceRecorder& recorder) {
  google::protobuf::RepeatedPtrField<allocation_recorder::MemoryOperation> data;

  for (size_t operation_index = 0; operation_index < recorder.size();
       ++operation_index) {
    data.Add(ConvertSingleRecord(recorder[operation_index]));
  }

  return data;
}
}  // namespace

allocation_recorder::Payload CreatePayloadWithMemoryOperationReport(
    const base::debug::tracer::AllocationTraceRecorder& recorder) {
  allocation_recorder::Payload full_report;

  *(full_report.mutable_operation_report()->mutable_statistics()) =
      ConvertRecorderStatistics(recorder.GetRecorderStatistics());

  (*full_report.mutable_operation_report()->mutable_memory_operations()) =
      ConvertMemoryOperations(recorder);

  return full_report;
}

allocation_recorder::Payload CreatePayloadWithProcessingFailures(
    base::span<const std::string_view> error_messages) {
  allocation_recorder::Payload full_report;
  auto& destination_messages =
      *(full_report.mutable_processing_failures()->mutable_messages());

  for (const auto error_message : error_messages) {
    destination_messages.Add(std::string(error_message));
  }

  return full_report;
}

}  // namespace allocation_recorder::crash_handler
