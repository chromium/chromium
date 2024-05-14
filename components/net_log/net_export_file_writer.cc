// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/net_log/net_export_file_writer.h"

#include <memory>
#include <set>
#include <string_view>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/observer_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/net_log/chrome_net_log.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace net_log {

namespace {

// Path of logs relative to default temporary directory given by
// base::GetTempDir(). Must be kept in sync with
// chrome/android/java/res/xml/file_paths.xml. Only used if not saving log file
// to a custom path.
const base::FilePath::CharType kLogRelativePath[] =
    FILE_PATH_LITERAL("net-export/chrome-net-export-log.json");

// Contains file-related initialization tasks for NetExportFileWriter.
NetExportFileWriter::DefaultLogPathResults SetUpDefaultLogPath(
    const NetExportFileWriter::DirectoryGetter& default_log_base_dir_getter) {
  NetExportFileWriter::DefaultLogPathResults results;
  results.default_log_path_success = false;
  results.log_exists = false;

  base::FilePath default_base_dir;
  if (!default_log_base_dir_getter.Run(&default_base_dir))
    return results;

  results.default_log_path = default_base_dir.Append(kLogRelativePath);
  if (!base::CreateDirectoryAndGetError(results.default_log_path.DirName(),
                                        nullptr))
    return results;

  results.log_exists = base::PathExists(results.default_log_path);
  results.default_log_path_success = true;
  return results;
}

base::FilePath GetPathIfExists(const base::FilePath& path) {
  if (!base::PathExists(path))
    return base::FilePath();
  return path;
}

scoped_refptr<base::SequencedTaskRunner> CreateFileTaskRunner() {
  // The tasks posted to this sequenced task runner do synchronous File I/O.
  //
  // These operations can be skipped on shutdown since FileNetLogObserver's API
  // doesn't require things to have completed until notified of completion.
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
}

}  // namespace

NetExportFileWriter::NetExportFileWriter()
    : state_(STATE_UNINITIALIZED),
      log_exists_(false),
      log_capture_mode_known_(false),
      log_capture_mode_(net::NetLogCaptureMode::kDefault),
      default_log_base_dir_getter_(base::BindRepeating(&base::GetTempDir)) {}

NetExportFileWriter::~NetExportFileWriter() {
  if (net_log_exporter_) {
    net_log_exporter_->Stop(base::Value::Dict(), base::DoNothing());
  }
}

void NetExportFileWriter::AddObserver(StateObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  state_observer_list_.AddObserver(observer);
}

void NetExportFileWriter::RemoveObserver(StateObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  state_observer_list_.RemoveObserver(observer);
}

void NetExportFileWriter::Initialize() {
  DCHECK(thread_checker_.CalledOnValidThread());

  file_task_runner_ = CreateFileTaskRunner();

  if (state_ != STATE_UNINITIALIZED)
    return;

  state_ = STATE_INITIALIZING;

  NotifyStateObserversAsync();

  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&SetUpDefaultLogPath, default_log_base_dir_getter_),
      base::BindOnce(&NetExportFileWriter::SetStateAfterSetUpDefaultLogPath,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NetExportFileWriter::StartNetLog(
    const base::FilePath& log_path,
    net::NetLogCaptureMode capture_mode,
    uint64_t max_file_size,
    const base::CommandLine::StringType& command_line_string,
    const std::string& channel_string,
    network::mojom::NetworkContext* network_context) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(file_task_runner_);

  if (state_ != STATE_NOT_LOGGING)
    return;

  if (!log_path.empty())
    log_path_ = log_path;

  DCHECK(!log_path_.empty());

  state_ = STATE_STARTING_LOG;

  NotifyStateObserversAsync();

  network_context->CreateNetLogExporter(
      net_log_exporter_.BindNewPipeAndPassReceiver());
  base::Value::Dict custom_constants =
      GetPlatformConstantsForNetLog(command_line_string, channel_string);

  net_log_exporter_.set_disconnect_handler(base::BindOnce(
      &NetExportFileWriter::OnConnectionError, base::Unretained(this)));

  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&NetExportFileWriter::CreateOutputFile, log_path_),
      base::BindOnce(&NetExportFileWriter::StartNetLogAfterCreateFile,
                     weak_ptr_factory_.GetWeakPtr(), capture_mode,
                     max_file_size, std::move(custom_constants)));
}

void NetExportFileWriter::StartNetLogAfterCreateFile(
    net::NetLogCaptureMode capture_mode,
    uint64_t max_file_size,
    base::Value::Dict custom_constants,
    base::File output_file) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(STATE_STARTING_LOG, state_);

  // TODO(morlovich): Communicate file open trouble better
  // (https://crbug.com/838977)
  if (!output_file.IsValid()) {
    ResetExporterThenSetStateNotLogging();
    return;
  }

  // It's possible that the network service crashed in the window between
  // StartNetLog and here. In that case, OnConnectionError will have closed
  // |net_log_exporter_|.
  if (!net_log_exporter_)
    return;

  // base::Unretained(this) is safe here since |net_log_exporter_| is owned by
  // |this| and is a mojo InterfacePtr, which guarantees callback cancellation
  // upon its destruction.
  net_log_exporter_->Start(
      std::move(output_file), std::move(custom_constants), capture_mode,
      max_file_size,
      base::BindOnce(&NetExportFileWriter::OnStartResult,
                     base::Unretained(this), capture_mode));
}

void NetExportFileWriter::OnStartResult(net::NetLogCaptureMode capture_mode,
                                        int result) {
  if (result == net::OK) {
    state_ = STATE_LOGGING;
    log_exists_ = true;
    log_capture_mode_known_ = true;
    log_capture_mode_ = capture_mode;

    NotifyStateObservers();
  } else {
    ResetExporterThenSetStateNotLogging();
  }
}

void NetExportFileWriter::StopNetLog(base::Value::Dict polled_data) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ != STATE_LOGGING)
    return;

  state_ = STATE_STOPPING_LOG;

  NotifyStateObserversAsync();

  // base::Unretained(this) is safe here since |net_log_exporter_| is owned by
  // |this| and is a mojo InterfacePtr, which guarantees callback cancellation
  // upon its destruction.
  net_log_exporter_->Stop(std::move(polled_data),
                          base::BindOnce(&NetExportFileWriter::OnStopResult,
                                         base::Unretained(this)));
}

void NetExportFileWriter::OnStopResult(int result) {
  ResetExporterThenSetStateNotLogging();
}

void NetExportFileWriter::OnConnectionError() {
  ResetExporterThenSetStateNotLogging();
}

base::Value::Dict NetExportFileWriter::GetState() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::Value::Dict dict;
  dict.Set("file", base::UTF16ToUTF8(log_path_.LossyDisplayName()));

  std::string_view state_string;
  switch (state_) {
    case STATE_UNINITIALIZED:
      state_string = "UNINITIALIZED";
      break;
    case STATE_INITIALIZING:
      state_string = "INITIALIZING";
      break;
    case STATE_NOT_LOGGING:
      state_string = "NOT_LOGGING";
      break;
    case STATE_STARTING_LOG:
      state_string = "STARTING_LOG";
      break;
    case STATE_LOGGING:
      state_string = "LOGGING";
      break;
    case STATE_STOPPING_LOG:
      state_string = "STOPPING_LOG";
      break;
  }
  dict.Set("state", state_string);

  dict.Set("logExists", log_exists_);
  dict.Set("logCaptureModeKnown", log_capture_mode_known_);
  dict.Set("captureMode", CaptureModeToString(log_capture_mode_));

  return dict;
}

void NetExportFileWriter::GetFilePathToCompletedLog(
    FilePathCallback path_callback) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!(log_exists_ && state_ == STATE_NOT_LOGGING)) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(path_callback), base::FilePath()));
    return;
  }

  DCHECK(file_task_runner_);
  DCHECK(!log_path_.empty());

  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&GetPathIfExists, log_path_),
      std::move(path_callback));
}

std::string NetExportFileWriter::CaptureModeToString(
    net::NetLogCaptureMode capture_mode) {
  if (capture_mode == net::NetLogCaptureMode::kDefault)
    return "STRIP_PRIVATE_DATA";
  if (capture_mode == net::NetLogCaptureMode::kIncludeSensitive)
    return "NORMAL";
  if (capture_mode == net::NetLogCaptureMode::kEverything)
    return "LOG_BYTES";
  NOTREACHED_IN_MIGRATION();
  return "STRIP_PRIVATE_DATA";
}

net::NetLogCaptureMode NetExportFileWriter::CaptureModeFromString(
    const std::string& capture_mode_string) {
  if (capture_mode_string == "STRIP_PRIVATE_DATA")
    return net::NetLogCaptureMode::kDefault;
  if (capture_mode_string == "NORMAL")
    return net::NetLogCaptureMode::kIncludeSensitive;
  if (capture_mode_string == "LOG_BYTES")
    return net::NetLogCaptureMode::kEverything;
  NOTREACHED_IN_MIGRATION();
  return net::NetLogCaptureMode::kDefault;
}

void NetExportFileWriter::SetDefaultLogBaseDirectoryGetterForTest(
    const DirectoryGetter& getter) {
  default_log_base_dir_getter_ = getter;
}

void NetExportFileWriter::NotifyStateObservers() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::Value::Dict state = GetState();
  for (StateObserver& observer : state_observer_list_) {
    observer.OnNewState(state);
  }
}

void NetExportFileWriter::NotifyStateObserversAsync() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&NetExportFileWriter::NotifyStateObservers,
                                weak_ptr_factory_.GetWeakPtr()));
}

void NetExportFileWriter::SetStateAfterSetUpDefaultLogPath(
    const DefaultLogPathResults& set_up_default_log_path_results) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(STATE_INITIALIZING, state_);

  if (set_up_default_log_path_results.default_log_path_success) {
    state_ = STATE_NOT_LOGGING;
    log_path_ = set_up_default_log_path_results.default_log_path;
    log_exists_ = set_up_default_log_path_results.log_exists;
    DCHECK(!log_capture_mode_known_);
  } else {
    state_ = STATE_UNINITIALIZED;
  }
  NotifyStateObservers();
}

void NetExportFileWriter::ResetExporterThenSetStateNotLogging() {
  DCHECK(thread_checker_.CalledOnValidThread());
  net_log_exporter_.reset();
  state_ = STATE_NOT_LOGGING;

  NotifyStateObservers();
}

base::File NetExportFileWriter::CreateOutputFile(base::FilePath path) {
  return base::File(path,
                    base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
}

}  // namespace net_log
