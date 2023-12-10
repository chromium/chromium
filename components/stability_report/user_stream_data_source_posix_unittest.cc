// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/stability_report/user_stream_data_source_posix.h"

#include "base/files/file_util.h"
#include "base/process/process_metrics.h"
#include "base/test/test_file_util.h"
#include "components/stability_report/test/stability_report_reader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/crashpad/crashpad/snapshot/test/test_process_snapshot.h"

namespace stability_report {

namespace {

// Tests whether the user stream correctly populates the StabilityReport with
// the number of open file descriptors by opening a file in the current
// (snapshotted) process.
TEST(StabilityReportUserStreamDataSourceTest, GetOpenFDs) {
  // Set up the test snapshot with the current process.
  crashpad::test::TestProcessSnapshot process_snapshot;
  const base::ProcessId process_id = base::Process::Current().Pid();
  process_snapshot.SetProcessID(process_id);

  std::unique_ptr<base::ProcessMetrics> metrics =
#if !BUILDFLAG(IS_MAC)
      base::ProcessMetrics::CreateProcessMetrics(process_id);
#else
      base::ProcessMetrics::CreateProcessMetrics(process_id, nullptr);
#endif  // !BUILDFLAG(IS_MAC)
  const int fd_count = metrics->GetOpenFdCount();
  EXPECT_GE(fd_count, 0);

  // Open an additional file in the current process.
  const base::FilePath temp_dir = base::CreateUniqueTempDirectoryScopedToTest();
  base::File file(temp_dir.AppendASCII("file"),
                  base::File::FLAG_CREATE | base::File::FLAG_WRITE);

  // Capture the snapshot stream data.
  UserStreamDataSourcePosix source;
  std::unique_ptr<crashpad::MinidumpUserExtensionStreamDataSource> data_source =
      source.ProduceStreamData(&process_snapshot);
  ASSERT_TRUE(data_source);

  test::StabilityReportReader reader;
  ASSERT_TRUE(data_source->ReadStreamData(&reader));

  // Confirm the value captured is one more than the process had open initially.
  ASSERT_EQ(reader.report().process_states_size(), 1);
  const ProcessState& process_state = reader.report().process_states(0);
  EXPECT_EQ(process_state.process_id(), process_snapshot.ProcessID());
  EXPECT_TRUE(process_state.has_file_system_state());
  EXPECT_TRUE(process_state.file_system_state().has_posix_file_system_state());
  EXPECT_EQ(process_state.file_system_state()
                .posix_file_system_state()
                .open_file_descriptors(),
            base::checked_cast<unsigned int>(fd_count + 1));
}

}  // namespace

}  // namespace stability_report
