// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/remote_commands/remote_command_job.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/syslog_logging.h"
#include "device_management_backend.pb.h"

namespace policy {

namespace {

constexpr base::TimeDelta kDefaultCommandTimeout = base::Minutes(10);
constexpr base::TimeDelta kDefaultCommandExpirationTime = base::Minutes(10);

std::string ToString(enterprise_management::RemoteCommand::Type type) {
#define CASE(_name)                                 \
  case enterprise_management::RemoteCommand::_name: \
    return #_name;

  switch (type) {
    CASE(COMMAND_ECHO_TEST);
    CASE(DEVICE_REBOOT);
    CASE(DEVICE_SCREENSHOT);
    CASE(DEVICE_SET_VOLUME);
    CASE(DEVICE_FETCH_STATUS);
    CASE(USER_ARC_COMMAND);
    CASE(DEVICE_WIPE_USERS);
    CASE(DEVICE_START_CRD_SESSION);
    CASE(DEVICE_REMOTE_POWERWASH);
    CASE(DEVICE_REFRESH_ENTERPRISE_MACHINE_CERTIFICATE);
    CASE(DEVICE_GET_AVAILABLE_DIAGNOSTIC_ROUTINES);
    CASE(DEVICE_RUN_DIAGNOSTIC_ROUTINE);
    CASE(DEVICE_GET_DIAGNOSTIC_ROUTINE_UPDATE);
    CASE(BROWSER_CLEAR_BROWSING_DATA);
    CASE(DEVICE_RESET_EUICC);
    CASE(BROWSER_ROTATE_ATTESTATION_CREDENTIAL);
    CASE(FETCH_CRD_AVAILABILITY_INFO);
    CASE(FETCH_SUPPORT_PACKET);
  }
  return base::StringPrintf("Unknown type %i", type);
#undef CASE
}

RemoteCommandJob::Status ResultTypeToStatus(ResultType result) {
  switch (result) {
    case ResultType::kSuccess:
      return RemoteCommandJob::SUCCEEDED;
    case ResultType::kFailure:
      return RemoteCommandJob::FAILED;
    case ResultType::kAcked:
      return RemoteCommandJob::ACKED;
  }
}

}  // namespace

RemoteCommandJob::~RemoteCommandJob() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (status_ == RUNNING) {
    Terminate();
  }
}

bool RemoteCommandJob::Init(
    base::TimeTicks now,
    const enterprise_management::RemoteCommand& command,
    const enterprise_management::SignedData& signed_command) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(NOT_INITIALIZED, status_);

  status_ = INVALID;

  if (!command.has_type() || !command.has_command_id()) {
    return false;
  }
  DCHECK_EQ(command.type(), GetType());

  unique_id_ = command.command_id();
  signed_command_ = signed_command;

  if (command.has_age_of_command()) {
    // Use age of command provided by server to estimate the command issued time
    // as a local TimeTick. We need to store issued time instead of age of
    // command, since the execution time of command might be different from the
    // time we got it from server.
    // It's just an estimation since we lost the time the response was
    // transmitted over the network.
    issued_time_ = now - base::Milliseconds(command.age_of_command());
  } else {
    SYSLOG(WARNING) << "No age_of_command provided by server for command "
                    << unique_id_ << ".";
    // Otherwise, assuming the command was issued just now.
    issued_time_ = now;
  }

  if (!ParseCommandPayload(command.payload())) {
    // payload may contain crypto key, thus only enabled for debugging mode.
    SYSLOG(ERROR) << "Unable to parse command payload for type "
                  << command.type();
    DLOG(ERROR) << "Command payload: " << command.payload();
    return false;
  }

  SYSLOG(INFO) << "Remote command type " << ToString(command.type()) << " ("
               << command.type() << ")"
               << " with id " << command.command_id() << " initialized.";

  status_ = NOT_STARTED;
  return true;
}

bool RemoteCommandJob::Run(base::Time now,
                           base::TimeTicks now_ticks,
                           FinishedCallback finished_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (status_ == INVALID) {
    SYSLOG(ERROR) << "Remote command " << unique_id_ << " is invalid.";
    return false;
  }

  DCHECK_EQ(NOT_STARTED, status_);

  if (IsExpired(now_ticks)) {
    SYSLOG(ERROR) << "Remote command " << unique_id_
                  << " expired (it was issued " << now_ticks - issued_time_
                  << " ago).";
    status_ = EXPIRED;
    return false;
  }

  execution_started_time_ = now;
  status_ = RUNNING;
  finished_callback_ = std::move(finished_callback);

  RunImpl(
      base::BindOnce(&RemoteCommandJob::OnCommandExecutionFinishedWithResult,
                     weak_factory_.GetWeakPtr()));

  // The command is expected to run asynchronously.
  DCHECK_EQ(RUNNING, status_);

  return true;
}

void RemoteCommandJob::Terminate() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (IsExecutionFinished()) {
    return;
  }

  DCHECK_EQ(RUNNING, status_);

  status_ = TERMINATED;
  weak_factory_.InvalidateWeakPtrs();

  TerminateImpl();

  if (finished_callback_) {
    std::move(finished_callback_).Run();
  }
}

base::TimeDelta RemoteCommandJob::GetCommandTimeout() const {
  return kDefaultCommandTimeout;
}

std::optional<enterprise_management::RemoteCommandResult::ResultType>
RemoteCommandJob::GetResult() const {
  switch (status_) {
    case SUCCEEDED:
      return enterprise_management::
          RemoteCommandResult_ResultType_RESULT_SUCCESS;
    case FAILED:
      return enterprise_management::
          RemoteCommandResult_ResultType_RESULT_FAILURE;
    case ACKED:
      // We don't send any result when the command is in `ACKED` state.
      return std::nullopt;
    // Result type is `RESULT_IGNORED` unless the command has finished execution
    // with either success or failure.
    default:
      return enterprise_management::
          RemoteCommandResult_ResultType_RESULT_IGNORED;
  }
}

bool RemoteCommandJob::IsExecutionFinished() const {
  return status_ == SUCCEEDED || status_ == FAILED || status_ == TERMINATED;
}

std::unique_ptr<std::string> RemoteCommandJob::GetResultPayload() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(status_ == SUCCEEDED || status_ == FAILED || status_ == ACKED);

  if (!result_payload_.has_value()) {
    return nullptr;
  }

  return std::make_unique<std::string>(std::move(result_payload_).value());
}

RemoteCommandJob::RemoteCommandJob() : status_(NOT_INITIALIZED) {}

bool RemoteCommandJob::ParseCommandPayload(const std::string& command_payload) {
  return true;
}

bool RemoteCommandJob::IsExpired(base::TimeTicks now) {
  return now > issued_time() + kDefaultCommandExpirationTime;
}

void RemoteCommandJob::TerminateImpl() {}

void RemoteCommandJob::OnCommandExecutionFinishedWithResult(
    ResultType result,
    std::optional<std::string> result_payload) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(RUNNING, status_);
  status_ = ResultTypeToStatus(result);

  result_payload_ = std::move(result_payload);

  if (finished_callback_) {
    std::move(finished_callback_).Run();
  }
}

}  // namespace policy
