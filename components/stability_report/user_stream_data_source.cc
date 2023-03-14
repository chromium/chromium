// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/stability_report/user_stream_data_source.h"

#include <windows.h>

// Must be included after windows.h.
#include <psapi.h>

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/dcheck_is_on.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/memory.h"
#include "base/process/process.h"
#include "components/stability_report/stability_report.pb.h"
#include "third_party/crashpad/crashpad/minidump/minidump_user_extension_stream_data_source.h"
#include "third_party/crashpad/crashpad/snapshot/exception_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/process_snapshot.h"

namespace stability_report {

namespace {

// The stream type assigned to the minidump stream that holds the serialized
// stability report.
// Note: the value was obtained by adding 1 to the stream type used for holding
// the SyzyAsan proto.
constexpr uint32_t kStreamType = 0x4B6B0002;

// System memory metrics are reported in pages. Use this page size to scale
// process memory metrics to the same units.
constexpr size_t kPageSize = 4096;

// A data source that holds a serialized StabilityReport.
class StabilityReportDataSource final
    : public crashpad::MinidumpUserExtensionStreamDataSource {
 public:
  explicit StabilityReportDataSource(const StabilityReport& report);
  ~StabilityReportDataSource() final = default;

  StabilityReportDataSource(const StabilityReportDataSource&) = delete;
  StabilityReportDataSource& operator=(const StabilityReportDataSource&) =
      delete;

  size_t StreamDataSize() final { return data_.size(); }

  bool ReadStreamData(Delegate* delegate) final;

 private:
  std::string data_;
};

StabilityReportDataSource::StabilityReportDataSource(
    const StabilityReport& report)
    : crashpad::MinidumpUserExtensionStreamDataSource(kStreamType),
      data_(report.SerializeAsString()) {
  // On error, SerializeAsString() will return an empty string which will
  // cause ReadStreamData() to harmlessly return no data.
}

bool StabilityReportDataSource::ReadStreamData(Delegate* delegate) {
  return delegate->ExtensionStreamDataSourceRead(data_.data(), data_.size());
}

// Adds system metrics to `report`.
void CollectSystemPerformanceMetrics(StabilityReport* report) {
  // Grab system commit memory. Best effort.
  PERFORMANCE_INFORMATION perf_info = {sizeof(perf_info)};
  if (!::GetPerformanceInfo(&perf_info, sizeof(perf_info))) {
    return;
  }
  SystemMemoryState::WindowsMemory* memory_state =
      report->mutable_system_memory_state()->mutable_windows_memory();
  memory_state->set_system_commit_limit(perf_info.CommitLimit);
  memory_state->set_system_commit_remaining(perf_info.CommitLimit -
                                            perf_info.CommitTotal);
  memory_state->set_system_handle_count(perf_info.HandleCount);
  // The process memory metrics won't be scaled correctly with an unexpected
  // page size.
  DCHECK_EQ(perf_info.PageSize, kPageSize);
}

// Adds metrics for the process in `process_snapshot` to `report`.
void CollectProcessPerformanceMetrics(
    const crashpad::ProcessSnapshot& process_snapshot,
    StabilityReport* report) {
  const base::ProcessId process_id = process_snapshot.ProcessID();

#if DCHECK_IS_ON()
  // Ensure no ProcessState was created yet for the process in question.
  for (const ProcessState& process_state : report->process_states()) {
    DCHECK_NE(process_state.process_id(), process_id);
  }
#endif

  ProcessState* process_state = report->add_process_states();
  process_state->set_process_id(process_id);

  ProcessState::MemoryState::WindowsMemory* memory_state =
      process_state->mutable_memory_state()->mutable_windows_memory();

  // Grab the requested allocation size in case of OOM exception.
  const crashpad::ExceptionSnapshot* const exception =
      process_snapshot.Exception();
  if (exception && exception->Exception() == base::win::kOomExceptionCode &&
      !exception->Codes().empty()) {
    // The first parameter, if present, is the size of the allocation attempt.
    // Note Codes() contains 64-bit values but `process_allocation_attempt` is
    // a uint32.
    memory_state->set_process_allocation_attempt(
        base::saturated_cast<uint32_t>(exception->Codes().front()));
  }

  const base::Process process = base::Process::OpenWithAccess(
      process_id, PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ);
  if (!process.IsValid()) {
    return;
  }

  PROCESS_MEMORY_COUNTERS_EX process_memory = {sizeof(process_memory)};
  if (::GetProcessMemoryInfo(
          process.Handle(),
          reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&process_memory),
          sizeof(process_memory))) {
    // This is in units of bytes, re-scale to pages for consistency with system
    // metrics.
    memory_state->set_process_private_usage(process_memory.PrivateUsage /
                                            kPageSize);
    memory_state->set_process_peak_workingset_size(
        process_memory.PeakWorkingSetSize / kPageSize);
    memory_state->set_process_peak_pagefile_usage(
        process_memory.PeakPagefileUsage / kPageSize);
  }

  DWORD process_handle_count = 0;
  if (::GetProcessHandleCount(process.Handle(), &process_handle_count)) {
    memory_state->set_process_handle_count(process_handle_count);
  }
}

}  // namespace

std::unique_ptr<crashpad::MinidumpUserExtensionStreamDataSource>
UserStreamDataSource::ProduceStreamData(
    crashpad::ProcessSnapshot* process_snapshot) {
  DCHECK(process_snapshot);

  StabilityReport report;
  CollectSystemPerformanceMetrics(&report);
  CollectProcessPerformanceMetrics(*process_snapshot, &report);

  return std::make_unique<StabilityReportDataSource>(report);
}

}  // namespace stability_report
