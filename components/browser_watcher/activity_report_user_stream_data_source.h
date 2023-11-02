// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_WATCHER_ACTIVITY_REPORT_USER_STREAM_DATA_SOURCE_H_
#define COMPONENTS_BROWSER_WATCHER_ACTIVITY_REPORT_USER_STREAM_DATA_SOURCE_H_

#include <memory>

#include "base/files/file_path.h"
#include "third_party/crashpad/crashpad/handler/user_stream_data_source.h"

namespace crashpad {
class MinidumpUserExtensionStreamDataSource;
class ProcessSnapshot;
}  // namespace crashpad

namespace browser_watcher {

// Collects stability instrumentation corresponding to a ProcessSnapshot and
// makes it available to the crash handler.
class ActivityReportUserStreamDataSource
    : public crashpad::UserStreamDataSource {
 public:
  explicit ActivityReportUserStreamDataSource(
      const base::FilePath& user_data_dir);

  ActivityReportUserStreamDataSource(
      const ActivityReportUserStreamDataSource&) = delete;
  ActivityReportUserStreamDataSource& operator=(
      const ActivityReportUserStreamDataSource&) = delete;

  std::unique_ptr<crashpad::MinidumpUserExtensionStreamDataSource>
  ProduceStreamData(crashpad::ProcessSnapshot* process_snapshot) override;

 private:
  base::FilePath user_data_dir_;
};

}  // namespace browser_watcher

#endif  // COMPONENTS_BROWSER_WATCHER_ACTIVITY_REPORT_USER_STREAM_DATA_SOURCE_H_
