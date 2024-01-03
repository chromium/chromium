// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/allocation_recorder/crash_handler/payload.h"

#include "base/allocator/dispatcher/notification_data.h"
#include "base/allocator/dispatcher/subsystem.h"
#include "base/bits.h"
#include "base/containers/span.h"
#include "base/debug/allocation_trace.h"
#include "base/ranges/algorithm.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::allocator::dispatcher::AllocationNotificationData;
using base::allocator::dispatcher::AllocationSubsystem;
using base::allocator::dispatcher::FreeNotificationData;
using base::debug::tracer::AllocationTraceRecorder;
using base::debug::tracer::AllocationTraceRecorderStatistics;
using base::debug::tracer::OperationRecord;
using testing::TestWithParam;

namespace allocation_recorder::crash_handler {
namespace {

void CreateFakeAllocationData(AllocationTraceRecorder& recorder,
                              uint64_t number_of_entries) {
  for (uint64_t entry_counter = 0; entry_counter < number_of_entries;
       ++entry_counter) {
    if (entry_counter & 0x1) {
      recorder.OnFree(
          FreeNotificationData(reinterpret_cast<void*>(entry_counter),
                               AllocationSubsystem::kPartitionAllocator));
    } else {
      recorder.OnAllocation(
          AllocationNotificationData(&recorder, entry_counter, nullptr,
                                     AllocationSubsystem::kPartitionAllocator));
    }
  }
}

void VerifyAllocationEntriesAreEqual(
    const base::debug::tracer::OperationRecord& source_entry,
    const ::allocation_recorder::MemoryOperation& report_entry) {
  EXPECT_EQ(reinterpret_cast<uint64_t>(source_entry.GetAddress()),
            report_entry.address());

  if (source_entry.GetOperationType() ==
      base::debug::tracer::OperationType::kAllocation) {
    ASSERT_TRUE(report_entry.has_size());
    EXPECT_EQ(source_entry.GetSize(), report_entry.size());
  } else {
    ASSERT_FALSE(report_entry.has_size());
  }

  switch (source_entry.GetOperationType()) {
    case base::debug::tracer::OperationType::kAllocation:
      EXPECT_EQ(report_entry.operation_type(),
                ::allocation_recorder::OperationType::ALLOCATION);
      break;
    case base::debug::tracer::OperationType::kFree:
      EXPECT_EQ(report_entry.operation_type(),
                ::allocation_recorder::OperationType::FREE);
      break;
    case base::debug::tracer::OperationType::kNone:
      ASSERT_NE(source_entry.GetOperationType(),
                base::debug::tracer::OperationType::kNone);
  }

  ASSERT_TRUE(report_entry.has_stack_trace());
  ASSERT_LE(std::ssize(report_entry.stack_trace().frames()),
            std::ssize(source_entry.GetStackTrace()));

  const auto& report_frames = report_entry.stack_trace().frames();
  std::vector<const void*> converted_frames;
  base::ranges::transform(
      report_frames, std::back_inserter(converted_frames),
      [](const allocation_recorder::StackFrame& frame) {
        return reinterpret_cast<const void*>(frame.address());
      });

  const auto [converted_call_stack_mismatch, source_call_stack_mismatch] =
      std::mismatch(std::begin(converted_frames), std::end(converted_frames),
                    std::begin(source_entry.GetStackTrace()));

  ASSERT_EQ(converted_call_stack_mismatch, std::end(converted_frames));
  EXPECT_TRUE(std::all_of(source_call_stack_mismatch,
                          std::end(source_entry.GetStackTrace()),
                          [](const void* ptr) { return ptr == nullptr; }));
}

}  // namespace

class CreatePayloadWithMemoryOperationReportTest : public testing::Test {
 public:
  AllocationTraceRecorder& GetRecorder() { return *recorder_; }

 private:
  // The recorder under test. Depending on number and size of traces, it
  // requires quite a lot of space. Therefore, we create it on heap to avoid any
  // out-of-stack scenarios.
  std::unique_ptr<AllocationTraceRecorder> const recorder_ =
      std::make_unique<AllocationTraceRecorder>();
};

TEST_F(CreatePayloadWithMemoryOperationReportTest, Verify) {
  constexpr size_t number_of_entries = 1024;
  auto& recorder = GetRecorder();

  CreateFakeAllocationData(recorder, number_of_entries);

  ASSERT_EQ(recorder.size(), number_of_entries);

  const allocation_recorder::Payload payload =
      CreatePayloadWithMemoryOperationReport(recorder);

  ASSERT_TRUE(payload.operation_report().has_statistics());
  EXPECT_EQ(
      number_of_entries,
      payload.operation_report().statistics().total_number_of_operations());
#if BUILDFLAG(ENABLE_ALLOCATION_TRACE_RECORDER_FULL_REPORTING)
  ASSERT_TRUE(
      payload.operation_report().statistics().has_total_number_of_collisions());
  EXPECT_EQ(
      0ul,
      payload.operation_report().statistics().total_number_of_collisions());
#endif

  ASSERT_TRUE(payload.has_operation_report());
  ASSERT_EQ(static_cast<int>(number_of_entries),
            payload.operation_report().memory_operations().size());

  for (size_t entry_index = 0; entry_index < number_of_entries; ++entry_index) {
    const auto& source_entry = recorder[entry_index];
    const auto& converted_entry =
        payload.operation_report().memory_operations().at(entry_index);

    VerifyAllocationEntriesAreEqual(source_entry, converted_entry);
  }
}

TEST_F(CreatePayloadWithMemoryOperationReportTest, VerifyErrorDataIsNotSet) {
  auto& recorder = GetRecorder();

  CreateFakeAllocationData(recorder, 8);

  const allocation_recorder::Payload payload =
      CreatePayloadWithMemoryOperationReport(recorder);

  EXPECT_FALSE(payload.has_processing_failures());
}

TEST(CreatePayloadWithProcessingFailuresTest, VerifySingleMessage) {
  const std::string_view message = "This is a very important message.";

  const allocation_recorder::Payload payload =
      CreatePayloadWithProcessingFailures(message);

  ASSERT_TRUE(payload.has_processing_failures());
  EXPECT_EQ(1, payload.processing_failures().messages().size());
  EXPECT_EQ(message, payload.processing_failures().messages().at(0));
}

TEST(CreatePayloadWithProcessingFailuresTest, VerifyMultipleMessages) {
  const std::string_view messages[] = {"This is a very important message.",
                                       "You'd better not ignore it."};

  const allocation_recorder::Payload payload =
      CreatePayloadWithProcessingFailures(base::make_span(messages));

  ASSERT_TRUE(payload.has_processing_failures());
  EXPECT_EQ(std::ssize(messages),
            payload.processing_failures().messages().size());

  for (int message_index = 0; const auto& source_message : messages) {
    EXPECT_EQ(source_message,
              payload.processing_failures().messages().at(message_index))
        << " at index=" << message_index;
    ++message_index;
  }
}

TEST(CreatePayloadWithProcessingFailuresTest, VerifyRegularReportDataIsNotSet) {
  const std::string_view message = "This is a very important message.";

  const allocation_recorder::Payload payload =
      CreatePayloadWithProcessingFailures(message);

  EXPECT_FALSE(payload.has_operation_report());
}

}  // namespace allocation_recorder::crash_handler
