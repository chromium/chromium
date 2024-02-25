// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STABILITY_REPORT_USER_STREAM_DATA_SOURCE_POSIX_H_
#define COMPONENTS_STABILITY_REPORT_USER_STREAM_DATA_SOURCE_POSIX_H_

#include "components/stability_report/user_stream_data_source.h"

namespace stability_report {

class UserStreamDataSourcePosix final : public UserStreamDataSource {
 public:
  UserStreamDataSourcePosix() = default;
  ~UserStreamDataSourcePosix() final = default;

  UserStreamDataSourcePosix(const UserStreamDataSourcePosix&) = delete;
  UserStreamDataSourcePosix& operator=(const UserStreamDataSourcePosix&) =
      delete;

  std::unique_ptr<crashpad::MinidumpUserExtensionStreamDataSource>
  ProduceStreamData(crashpad::ProcessSnapshot* process_snapshot) final;
};

}  // namespace stability_report

#endif  // COMPONENTS_STABILITY_REPORT_USER_STREAM_DATA_SOURCE_POSIX_H_
