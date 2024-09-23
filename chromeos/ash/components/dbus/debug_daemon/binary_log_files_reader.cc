// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/debug_daemon/binary_log_files_reader.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/dbus/common/pipe_reader.h"
#include "third_party/cros_system_api/dbus/debugd/dbus-constants.h"

namespace feedback {

BinaryLogFilesReader::BinaryLogFilesReader() = default;
BinaryLogFilesReader::~BinaryLogFilesReader() = default;

void BinaryLogFilesReader::GetFeedbackBinaryLogs(
    const cryptohome::AccountIdentifier& id,
    debugd::FeedbackBinaryLogType log_type,
    GetFeedbackBinaryLogsCallback callback) {
  CHECK(callback);
  const auto task_runner = base::ThreadPool::CreateTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});
  auto pipe_reader = std::make_unique<chromeos::PipeReader>(task_runner);

  // Sets up stream for data collection and returns the write end of the pipe if
  // stream was setup correctly. The write end will be passed to debugd which
  // will write data to it. On completion or any failure, OnIOComplete will be
  // called.
  base::ScopedFD pipe_write_end = pipe_reader->StartIO(base::BindOnce(
      &BinaryLogFilesReader::OnIOComplete, weak_ptr_factory_.GetWeakPtr(),
      log_type, std::move(pipe_reader), std::move(callback)));

  // Current implementation only fetches one log file.
  std::map<debugd::FeedbackBinaryLogType, base::ScopedFD> log_fd_map;
  log_fd_map[log_type] = std::move(pipe_write_end);
  // Pass the write end of the pipe to debugd to collect data.
  // OnGetFeedbackBinaryLogsCompleted will be called after debugd has started
  // writing logs to the pipe. The purpose of the callback is merely for
  // logging. The debugd method GetFeedbackBinaryLogs is async and will return
  // without waiting for IO completion. Once debugd finishes writing to the
  // pipe, it will close its write end. The read end of pipe will receive data
  // through the OnIOComplete callback. In case of timeout or other failures,
  // the write end will be closed and OnIOComplete will be called with empty
  // data.
  ash::DebugDaemonClient::Get()->GetFeedbackBinaryLogs(
      id, log_fd_map,
      base::BindOnce(&BinaryLogFilesReader::OnGetFeedbackBinaryLogsCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BinaryLogFilesReader::OnIOComplete(
    debugd::FeedbackBinaryLogType log_type,
    std::unique_ptr<chromeos::PipeReader> pipe_reader,
    GetFeedbackBinaryLogsCallback callback,
    std::optional<std::string> data) {
  CHECK(callback);
  // Shut down data collection.
  pipe_reader.reset();
  // Current implementation supports only one log type at a time. Therefore, it
  // is ok to run the callback here.
  BinaryLogsResponse response =
      std::make_unique<std::map<FeedbackBinaryLogType, std::string>>();
  response->emplace(log_type, data.value_or(std::string()));
  std::move(callback).Run(std::move(response));
}

void BinaryLogFilesReader::OnGetFeedbackBinaryLogsCompleted(bool succeeded) {
  if (!succeeded) {
    LOG(ERROR) << "GetFeedbackBinaryLogs failed.";
  }
}

}  // namespace feedback
