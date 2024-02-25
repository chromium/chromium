// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/stability_report/user_stream_data_source_win.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/process/memory.h"
#include "base/process/process.h"
#include "components/stability_report/stability_report.pb.h"
#include "components/stability_report/test/stability_report_reader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/crashpad/crashpad/snapshot/test/test_exception_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/test/test_process_snapshot.h"

namespace stability_report {

namespace {

constexpr uint64_t kExpectedAllocationAttempt = 12345;

using ::testing::Bool;
using ::testing::Combine;
using ::testing::TestWithParam;
using ::testing::Values;

enum class ExceptionCode {
  // No ExceptionSnapshot.
  kNone,
  // ExceptionSnapshot has a non-OOM code.
  kNotOOM,
  // ExceptionSnapshot with kOomExceptionCode and allocation info in Codes().
  kOOMWithAllocation,
  // ExceptionSnapshot with kOomExceptionCode but no allocation info in Codes().
  kOOMWithoutAllocation,
};

class StabilityReportUserStreamDataSourceTest
    : public TestWithParam<std::tuple<bool, ExceptionCode>> {
 protected:
  StabilityReportUserStreamDataSourceTest() {
    std::tie(is_valid_process_, exception_code_) = GetParam();
  }

  bool is_valid_process_;
  ExceptionCode exception_code_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         StabilityReportUserStreamDataSourceTest,
                         Combine(Bool(),
                                 Values(ExceptionCode::kNone,
                                        ExceptionCode::kNotOOM,
                                        ExceptionCode::kOOMWithAllocation,
                                        ExceptionCode::kOOMWithoutAllocation)));

TEST_P(StabilityReportUserStreamDataSourceTest, ReadProcess) {
  crashpad::test::TestProcessSnapshot process_snapshot;
  process_snapshot.SetProcessID(is_valid_process_
                                    ? base::Process::Current().Pid()
                                    : base::kNullProcessId);
  if (exception_code_ != ExceptionCode::kNone) {
    auto exception_snapshot =
        std::make_unique<crashpad::test::TestExceptionSnapshot>();
    switch (exception_code_) {
      case ExceptionCode::kNotOOM:
        // Set an arbitrary error code.
        exception_snapshot->SetException(ERROR_INVALID_HANDLE);
        exception_snapshot->SetCodes({kExpectedAllocationAttempt});
        break;
      case ExceptionCode::kOOMWithAllocation:
        exception_snapshot->SetException(base::win::kOomExceptionCode);
        exception_snapshot->SetCodes({kExpectedAllocationAttempt});
        break;
      case ExceptionCode::kOOMWithoutAllocation:
        exception_snapshot->SetException(base::win::kOomExceptionCode);
        exception_snapshot->SetCodes({});
        break;
      case ExceptionCode::kNone:
        // Should not be reached.
        FAIL();
    }
    process_snapshot.SetException(std::move(exception_snapshot));
  }

  // Collect a StabilityReport from `process_snapshot`.
  UserStreamDataSourceWin source;
  std::unique_ptr<crashpad::MinidumpUserExtensionStreamDataSource> data_source =
      source.ProduceStreamData(&process_snapshot);
  ASSERT_TRUE(data_source);

  // Read the StabilityReport back out of the stream data.
  test::StabilityReportReader reader;
  ASSERT_TRUE(data_source->ReadStreamData(&reader));

  // Validate ProcessState.
  ASSERT_EQ(reader.report().process_states_size(), 1);
  const ProcessState& process_state = reader.report().process_states(0);
  EXPECT_EQ(process_state.process_id(), process_snapshot.ProcessID());
  ASSERT_TRUE(process_state.has_memory_state());
  ASSERT_TRUE(process_state.memory_state().has_windows_memory());
  const ProcessState::MemoryState::WindowsMemory& process_memory =
      process_state.memory_state().windows_memory();
  const ProcessState::FileSystemState::WindowsFileSystemState& process_fs =
      process_state.file_system_state().windows_file_system_state();

  // Memory stats are only filled in if the process could be queried.
  if (is_valid_process_) {
    EXPECT_GT(process_memory.process_private_usage(), 0U);
    EXPECT_GT(process_memory.process_peak_workingset_size(), 0U);
    EXPECT_GT(process_memory.process_peak_pagefile_usage(), 0U);
    EXPECT_GT(process_fs.process_handle_count(), 0U);
  } else {
    EXPECT_FALSE(process_memory.has_process_private_usage());
    EXPECT_FALSE(process_memory.has_process_peak_workingset_size());
    EXPECT_FALSE(process_memory.has_process_peak_pagefile_usage());
    EXPECT_FALSE(process_fs.has_process_handle_count());
  }

  // Allocation attempt is only set on OOM when all info is available.
  if (exception_code_ == ExceptionCode::kOOMWithAllocation) {
    EXPECT_EQ(process_memory.process_allocation_attempt(),
              kExpectedAllocationAttempt);
  } else {
    EXPECT_FALSE(process_memory.has_process_allocation_attempt());
  }

  // Validate SystemMemoryState.
  ASSERT_TRUE(reader.report().has_system_memory_state());
  ASSERT_TRUE(reader.report().system_memory_state().has_windows_memory());
  const SystemMemoryState::WindowsMemory& system_memory =
      reader.report().system_memory_state().windows_memory();
  EXPECT_GT(system_memory.system_commit_limit(), 0U);
  EXPECT_GT(system_memory.system_commit_remaining(), 0U);
  EXPECT_GT(system_memory.system_handle_count(), 0U);
}

}  // namespace

}  // namespace stability_report
