// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_DEBUG_DAEMON_BINARY_LOG_FILES_READER_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_DEBUG_DAEMON_BINARY_LOG_FILES_READER_H_

#include <map>
#include <memory>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/common/pipe_reader.h"
#include "third_party/cros_system_api/dbus/debugd/dbus-constants.h"

namespace cryptohome {
class AccountIdentifier;
}

namespace feedback {

using debugd::FeedbackBinaryLogType;

// Helper class to fetch binary log files over dbus using debugd's
// GetFeedbackBinaryLogs method. The method can fetch multiple logs, one for
// each log types in one trip. Since currently there is one log type only, this
// class only supports fetching one log for now. It can be expanded when needed.
class COMPONENT_EXPORT(DEBUG_DAEMON) BinaryLogFilesReader {
 public:
  BinaryLogFilesReader();
  BinaryLogFilesReader(const BinaryLogFilesReader&) = delete;
  BinaryLogFilesReader& operator=(const BinaryLogFilesReader&) = delete;
  ~BinaryLogFilesReader();

  using BinaryLogsResponse =
      std::unique_ptr<std::map<FeedbackBinaryLogType, std::string>>;
  // Callback type for GetFeedbackBinaryLogs();
  using GetFeedbackBinaryLogsCallback =
      base::OnceCallback<void(BinaryLogsResponse logs_response)>;
  // Start calling debugd's GetFeedbackBinaryLogs method to fetch log files. The
  // callback will be invoked once fetching is completed.
  void GetFeedbackBinaryLogs(const cryptohome::AccountIdentifier& id,
                             debugd::FeedbackBinaryLogType log_type,
                             GetFeedbackBinaryLogsCallback callback);

 private:
  void OnIOComplete(debugd::FeedbackBinaryLogType log_type,
                    std::unique_ptr<chromeos::PipeReader> pipe_reader,
                    GetFeedbackBinaryLogsCallback callback,
                    std::optional<std::string> data);
  void OnGetFeedbackBinaryLogsCompleted(bool succeeded);

  base::WeakPtrFactory<BinaryLogFilesReader> weak_ptr_factory_{this};
};

}  // namespace feedback

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_DEBUG_DAEMON_BINARY_LOG_FILES_READER_H_
