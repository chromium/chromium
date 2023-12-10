// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/stability_report/user_stream_data_source_posix.h"

#include "base/process/process_metrics.h"
#include "components/stability_report/stability_report_data_source.h"
#include "third_party/crashpad/crashpad/snapshot/process_snapshot.h"

namespace stability_report {

namespace {

// Adds file descriptor information to the `process_state` reference.
void CollectFileDescriptorInfo(ProcessState& process_state,
                               const base::ProcessId process_id) {
  std::unique_ptr<base::ProcessMetrics> metrics =
#if !BUILDFLAG(IS_MAC)
      base::ProcessMetrics::CreateProcessMetrics(process_id);
#else
      base::ProcessMetrics::CreateProcessMetrics(process_id, nullptr);
#endif  // !BUILDFLAG(IS_MAC)
  ProcessState::FileSystemState::PosixFileSystemState* file_system_state =
      process_state.mutable_file_system_state()
          ->mutable_posix_file_system_state();
  file_system_state->set_open_file_descriptors(metrics->GetOpenFdCount());
}

}  // namespace

std::unique_ptr<crashpad::MinidumpUserExtensionStreamDataSource>
UserStreamDataSourcePosix::ProduceStreamData(
    crashpad::ProcessSnapshot* process_snapshot) {
  DCHECK(process_snapshot);

  StabilityReport report;
  const base::ProcessId process_id = process_snapshot->ProcessID();
  ProcessState& process_state = AddProcessForSnapshot(process_id, &report);
  CollectFileDescriptorInfo(process_state, process_id);

  return std::make_unique<StabilityReportDataSource>(report);
}

}  // namespace stability_report
