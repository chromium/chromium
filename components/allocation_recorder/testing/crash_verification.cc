// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/allocation_recorder/testing/crash_verification.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <memory>
#include <set>
#include <vector>

#include "base/debug/debugging_buildflags.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/ranges/algorithm.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/allocation_recorder/internal/internal.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/snapshot/minidump/minidump_stream.h"
#include "third_party/crashpad/crashpad/snapshot/minidump/process_snapshot_minidump.h"
#include "third_party/crashpad/crashpad/util/misc/uuid.h"

#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
#include "components/allocation_recorder/crash_handler/memory_operation_report.pb.h"
#endif

using testing::AssertionFailure;
using testing::AssertionResult;
using testing::AssertionSuccess;

namespace crashpad {
void PrintTo(const CrashReportDatabase::OperationStatus status,
             std::ostream* os) {
  switch (status) {
    case CrashReportDatabase::kNoError:
      *os << "CrashReportDatabase::OperationStatus::kNoError";
      break;
    case CrashReportDatabase::kReportNotFound:
      *os << "CrashReportDatabase::OperationStatus::kReportNotFound";
      break;
    case CrashReportDatabase::kFileSystemError:
      *os << "CrashReportDatabase::OperationStatus::kFileSystemError";
      break;
    case CrashReportDatabase::kDatabaseError:
      *os << "CrashReportDatabase::OperationStatus::kDatabaseError";
      break;
    case CrashReportDatabase::kBusyError:
      *os << "CrashReportDatabase::OperationStatus::kBusyError";
      break;
    case CrashReportDatabase::kCannotRequestUpload:
      *os << "CrashReportDatabase::OperationStatus::kCannotRequestUpload";
      break;
  }
}
}  // namespace crashpad

namespace {

class CrashpadIntegration {
 public:
  CrashpadIntegration() = default;
  ~CrashpadIntegration() {
    if (crash_report_database_) {
      ShutDown();
    }
  }

  void SetUp(const base::FilePath& crashpad_database_path);
  void ShutDown();

  // Get all MinidumpStreams whose stream type equals
  // |allocation_recorder::internal::kStreamDataType|.
  std::vector<const crashpad::MinidumpStream*> GetAllocationRecorderStreams();

  void CheckHasNoAllocationRecorderStream();

#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
  void GetPayload(allocation_recorder::Payload& payload);
#endif

 private:
  void CrashDatabaseHasReport();
  void ReportHasProcessSnapshot();

#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
  void GetAllocationRecorderStream(
      const crashpad::MinidumpStream*& allocation_recorder_stream);
#endif

  // Try and read the created report from the database of crash reports. Errors
  // such as failure to read the list of pending reports will always result in a
  // fatal error.
  void TryReadCreatedReport();

  base::FilePath database_dir_;

  std::unique_ptr<crashpad::CrashReportDatabase> crash_report_database_;

  std::unique_ptr<crashpad::CrashReportDatabase::Report> report_;
  std::unique_ptr<crashpad::ProcessSnapshotMinidump> minidump_process_snapshot_;
};

void CrashpadIntegration::SetUp(const base::FilePath& crashpad_database_path) {
  database_dir_ = crashpad_database_path;

  crash_report_database_ =
      crashpad::CrashReportDatabase::InitializeWithoutCreating(database_dir_);
  ASSERT_NE(crash_report_database_, nullptr)
      << "Failed to load crash database. database='" << database_dir_ << '\'';

  std::vector<crashpad::CrashReportDatabase::Report> reports;
  ASSERT_EQ(crashpad::CrashReportDatabase::kNoError,
            crash_report_database_->GetPendingReports(&reports))
      << "Failed to read list of old pending reports. database='"
      << database_dir_ << '\'';
  ASSERT_EQ(reports.size(), std::size_t{0})
      << "Expected no reports at setup time."
      << "Please choose a unique temporary crashpad_database_path.";
}

void CrashpadIntegration::ShutDown() {
  if (report_) {
    const auto report_deletion_result =
        crash_report_database_->DeleteReport(report_->uuid);
    EXPECT_EQ(crashpad::CrashReportDatabase::kNoError, report_deletion_result);
  }

  minidump_process_snapshot_ = {};
  report_ = {};
  crash_report_database_ = {};
  database_dir_ = {};
}

void CrashpadIntegration::CrashDatabaseHasReport() {
  // The Crashpad report might not have been written yet. Try to read the report
  // multiple times without asserting success. Only in the very last try we
  // assert that a new report is present.
  constexpr auto maximum_total_retry_duration = base::Seconds(5);
  constexpr auto wait_time_between_retries = base::Milliseconds(200);

  const auto time_out_to_last_nonfatal_try =
      base::Time::Now() + maximum_total_retry_duration;

  while (base::Time::Now() <= time_out_to_last_nonfatal_try) {
    ASSERT_NO_FATAL_FAILURE(TryReadCreatedReport());

    if (report_) {
      return;
    }

    base::PlatformThreadBase::Sleep(wait_time_between_retries);
  }

  ASSERT_NO_FATAL_FAILURE(TryReadCreatedReport());
  ASSERT_NE(report_, nullptr)
      << "Found no new report. database='" << database_dir_ << '\'';
}

void CrashpadIntegration::ReportHasProcessSnapshot() {
  crashpad::FileReader file_reader;

  minidump_process_snapshot_ =
      std::make_unique<crashpad::ProcessSnapshotMinidump>();

  ASSERT_TRUE(file_reader.Open(report_->file_path))
      << "Failed to open dump file. path='" << report_->file_path << '\'';

  ASSERT_TRUE(minidump_process_snapshot_->Initialize(&file_reader))
      << "Failed to initialize process snapshot "
         "from report file. path='"
      << report_->file_path << '\'';
}

std::vector<const crashpad::MinidumpStream*>
CrashpadIntegration::GetAllocationRecorderStreams() {
  const auto stream_has_correct_type_predicate =
      [](const crashpad::MinidumpStream* const stream) {
        return stream && stream->stream_type() ==
                             allocation_recorder::internal::kStreamDataType;
      };

  const auto& custom_minidump_streams =
      minidump_process_snapshot_->CustomMinidumpStreams();

  std::vector<const crashpad::MinidumpStream*> allocation_recorder_streams;

  std::copy_if(std::begin(custom_minidump_streams),
               std::end(custom_minidump_streams),
               std::back_inserter(allocation_recorder_streams),
               stream_has_correct_type_predicate);

  return allocation_recorder_streams;
}

void CrashpadIntegration::TryReadCreatedReport() {
  std::vector<crashpad::CrashReportDatabase::Report> reports;
  ASSERT_EQ(crashpad::CrashReportDatabase::kNoError,
            crash_report_database_->GetPendingReports(&reports))
      << "Failed to read list of pending reports. database='" << database_dir_
      << '\'';
  ASSERT_EQ(reports.size(), std::size_t{1});
  report_ = std::make_unique<crashpad::CrashReportDatabase::Report>(reports[0]);
}

void CrashpadIntegration::CheckHasNoAllocationRecorderStream() {
  ASSERT_NO_FATAL_FAILURE(CrashDatabaseHasReport());
  ASSERT_NO_FATAL_FAILURE(ReportHasProcessSnapshot());

  const auto allocation_recorder_streams = GetAllocationRecorderStreams();

  EXPECT_EQ(std::size(allocation_recorder_streams), 0ul)
      << "Found at least one allocation recorder stream.";
}

#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
void CrashpadIntegration::GetAllocationRecorderStream(
    const crashpad::MinidumpStream*& allocation_recorder_stream) {
  const auto allocation_recorder_streams = GetAllocationRecorderStreams();

  ASSERT_EQ(std::size(allocation_recorder_streams), 1ul)
      << "Didn't find expected number of allocation recorder streams.";
  ASSERT_NE(allocation_recorder_streams.front(), nullptr)
      << "The only allocation recorder stream is nullptr.";

  allocation_recorder_stream = allocation_recorder_streams.front();
}

void CrashpadIntegration::GetPayload(allocation_recorder::Payload& payload) {
  ASSERT_NO_FATAL_FAILURE(CrashDatabaseHasReport());
  ASSERT_NO_FATAL_FAILURE(ReportHasProcessSnapshot());

  const crashpad::MinidumpStream* allocation_recorder_stream;
  ASSERT_NO_FATAL_FAILURE(
      GetAllocationRecorderStream(allocation_recorder_stream));
  const std::vector<uint8_t>& data = allocation_recorder_stream->data();

  ASSERT_TRUE(payload.ParseFromArray(std::data(data), std::size(data)))
      << "Failed to parse recorder information "
         "from recorder stream.";
}
#endif
}  // namespace

namespace allocation_recorder::testing {

#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
void VerifyCrashCreatesCrashpadReportWithAllocationRecorderStream(
    const base::FilePath& crashpad_database_path,
    base::OnceClosure crash_function,
    base::OnceCallback<void(const allocation_recorder::Payload& payload)>
        payload_verification) {
  ASSERT_TRUE(crash_function);
  CrashpadIntegration crashpad_integration;
  ASSERT_NO_FATAL_FAILURE(crashpad_integration.SetUp(crashpad_database_path));
  ASSERT_NO_FATAL_FAILURE(std::move(crash_function).Run());

  allocation_recorder::Payload payload;
  ASSERT_NO_FATAL_FAILURE(crashpad_integration.GetPayload(payload));

  if (payload_verification) {
    ASSERT_NO_FATAL_FAILURE(std::move(payload_verification).Run(payload));
  }
  ASSERT_NO_FATAL_FAILURE(crashpad_integration.ShutDown());
}

void VerifyPayload(const bool expect_report_with_content,
                   const allocation_recorder::Payload& payload) {
  if (expect_report_with_content) {
    if (payload.has_processing_failures()) {
      const auto& failures = payload.processing_failures();
      const auto& messages = failures.messages();
      ASSERT_GT(messages.size(), 0);
      FAIL() << "Payload has unexpected processing failure:\n" << messages[0];
    }
    ASSERT_TRUE(payload.has_operation_report());
    const auto& operation_report = payload.operation_report();

    ASSERT_TRUE(operation_report.has_statistics());
    const auto& statistics = operation_report.statistics();

    EXPECT_GT(operation_report.memory_operations_size(), 0);
    EXPECT_GT(statistics.total_number_of_operations(), 0ul);

#if BUILDFLAG(ENABLE_ALLOCATION_TRACE_RECORDER_FULL_REPORTING)
    EXPECT_TRUE(statistics.has_total_number_of_collisions());
#endif
  } else {
    ASSERT_TRUE(payload.has_processing_failures());
    const auto& failures = payload.processing_failures();
    const auto& messages = failures.messages();
    ASSERT_EQ(messages.size(), 1);
    ASSERT_EQ(
        messages[0],
        "No annotation found! required-name=allocation-recorder-crash-info");
  }
}
#else
void VerifyCrashCreatesCrashpadReportWithoutAllocationRecorderStream(
    const base::FilePath& crashpad_database_path,
    base::OnceClosure crash_function) {
  ASSERT_TRUE(crash_function);
  CrashpadIntegration crashpad_integration;
  ASSERT_NO_FATAL_FAILURE(crashpad_integration.SetUp(crashpad_database_path));
  ASSERT_NO_FATAL_FAILURE(std::move(crash_function).Run());
  ASSERT_NO_FATAL_FAILURE(crashpad_integration.ShutDown());
}
#endif  // BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)

}  // namespace allocation_recorder::testing
