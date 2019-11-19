// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/remote_commands/remote_command_job.h"

#include <utility>

#include "base/bind.h"
#include "base/syslog_logging.h"

namespace policy {

namespace {

constexpr base::TimeDelta kDefaultCommandTimeout =
    base::TimeDelta::FromMinutes(10);
constexpr base::TimeDelta kDefaultCommandExpirationTime =
    base::TimeDelta::FromMinutes(10);

}  // namespace

RemoteCommandJob::~RemoteCommandJob() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (status_ == RUNNING)
    Terminate();
}

bool RemoteCommandJob::Init(
    base::TimeTicks now,
    const enterprise_management::RemoteCommand& command,
    const enterprise_management::SignedData* signed_command) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(NOT_INITIALIZED, status_);

  status_ = INVALID;

  if (!command.has_type() || !command.has_command_id())
    return false;
  DCHECK_EQ(command.type(), GetType());

  unique_id_ = command.command_id();
  if (signed_command)
    signed_command_ = *signed_command;

  if (command.has_age_of_command()) {
    // Use age of command provided by server to estimate the command issued time
    // as a local TimeTick. We need to store issued time instead of age of
    // command, since the execution time of command might be different from the
    // time we got it from server.
    // It's just an estimation since we lost the time the response was
    // transmitted over the network.
    issued_time_ =
        now - base::TimeDelta::FromMilliseconds(command.age_of_command());
  } else {
    SYSLOG(WARNING) << "No age_of_command provided by server for command "
                    << unique_id_ << ".";
    // Otherwise, assuming the command was issued just now.
    issued_time_ = now;
  }

  if (!ParseCommandPayload(command.payload())) {
    SYSLOG(ERROR) << "Unable to parse command payload for type "
                  << command.type() << ": " << command.payload();
    return false;
  }

  SYSLOG(INFO) << "Remote command type " << command.type() << " with id "
               << command.command_id() << " initialized.";

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
                     weak_factory_.GetWeakPtr(), true),
      base::BindOnce(&RemoteCommandJob::OnCommandExecutionFinishedWithResult,
                     weak_factory_.GetWeakPtr(), false));

  // The command is expected to run asynchronously.
  DCHECK_EQ(RUNNING, status_);

  return true;
}

void RemoteCommandJob::Terminate() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (IsExecutionFinished())
    return;

  DCHECK_EQ(RUNNING, status_);

  status_ = TERMINATED;
  weak_factory_.InvalidateWeakPtrs();

  TerminateImpl();

  if (finished_callback_)
    std::move(finished_callback_).Run();
}

base::TimeDelta RemoteCommandJob::GetCommandTimeout() const {
  return kDefaultCommandTimeout;
}

bool RemoteCommandJob::IsExecutionFinished() const {
  return status_ == SUCCEEDED || status_ == FAILED || status_ == TERMINATED;
}

std::unique_ptr<std::string> RemoteCommandJob::GetResultPayload() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(status_ == SUCCEEDED || status_ == FAILED);

  if (!result_payload_)
    return nullptr;

  return result_payload_->Serialize();
}

RemoteCommandJob::RemoteCommandJob() : status_(NOT_INITIALIZED) {}

bool RemoteCommandJob::ParseCommandPayload(const std::string& command_payload) {
  return true;
}

bool RemoteCommandJob::IsExpired(base::TimeTicks now) {
  return now > issued_time() + kDefaultCommandExpirationTime;
}

void RemoteCommandJob::TerminateImpl() {
}

void RemoteCommandJob::OnCommandExecutionFinishedWithResult(
    bool succeeded,
    std::unique_ptr<RemoteCommandJob::ResultPayload> result_payload) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(RUNNING, status_);
  status_ = succeeded ? SUCCEEDED : FAILED;

  result_payload_ = std::move(result_payload);

  if (finished_callback_)
    std::move(finished_callback_).Run();
}

}  // namespace policy
