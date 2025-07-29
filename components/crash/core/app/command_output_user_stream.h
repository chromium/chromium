// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CORE_APP_COMMAND_OUTPUT_USER_STREAM_H_
#define COMPONENTS_CRASH_CORE_APP_COMMAND_OUTPUT_USER_STREAM_H_

#include "third_party/crashpad/crashpad/handler/user_stream_data_source.h"

namespace crash_reporter {

// A custom minidump stream source that, when evidence of
// https://crbug.com/40064248 is observed, produces output from various commands
// that might be useful for troubleshooting.
//
// TODO(crbug.com/40064248): Remove this once sufficient information is
// collected.
class CommandOutputUserStream final : public crashpad::UserStreamDataSource {
 public:
  CommandOutputUserStream() = default;
  ~CommandOutputUserStream() final = default;

  CommandOutputUserStream(const CommandOutputUserStream&) = delete;
  CommandOutputUserStream& operator=(const CommandOutputUserStream&) = delete;

  std::unique_ptr<crashpad::MinidumpUserExtensionStreamDataSource>
  ProduceStreamData(crashpad::ProcessSnapshot* process_snapshot) final;
};

}  // namespace crash_reporter

#endif  // COMPONENTS_CRASH_CORE_APP_COMMAND_OUTPUT_USER_STREAM_H_
