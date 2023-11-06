// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STABILITY_REPORT_USER_STREAM_DATA_SOURCE_H_
#define COMPONENTS_STABILITY_REPORT_USER_STREAM_DATA_SOURCE_H_

#include "base/process/process.h"
#include "components/stability_report/stability_report.pb.h"
#include "third_party/crashpad/crashpad/handler/user_stream_data_source.h"

namespace crashpad {
class MinidumpUserExtensionStreamDataSource;
class ProcessSnapshot;
}  // namespace crashpad

namespace stability_report {

// Collects stability instrumentation corresponding to a ProcessSnapshot and
// makes it available to the crash handler as a serialized StabilityReport
// proto.
class UserStreamDataSource : public crashpad::UserStreamDataSource {
 public:
  UserStreamDataSource() = default;
  ~UserStreamDataSource() override = default;

  UserStreamDataSource(const UserStreamDataSource&) = delete;
  UserStreamDataSource& operator=(const UserStreamDataSource&) = delete;

  std::unique_ptr<crashpad::MinidumpUserExtensionStreamDataSource>
  ProduceStreamData(crashpad::ProcessSnapshot* process_snapshot) override = 0;
};

// Adds an entry for the given `process_id` to `report`.
ProcessState& AddProcessForSnapshot(const base::ProcessId process_id,
                                    StabilityReport* report);

}  // namespace stability_report

#endif  // COMPONENTS_STABILITY_REPORT_USER_STREAM_DATA_SOURCE_H_
