// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/remote_commands/remote_commands_service.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/stl_util.h"
#include "base/syslog_logging.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/core/common/remote_commands/remote_commands_factory.h"

namespace policy {

namespace em = enterprise_management;

RemoteCommandsService::RemoteCommandsService(
    std::unique_ptr<RemoteCommandsFactory> factory,
    CloudPolicyClient* client,
    CloudPolicyStore* store)
    : factory_(std::move(factory)), client_(client), store_(store) {
  DCHECK(client_);
  queue_.AddObserver(this);
}

RemoteCommandsService::~RemoteCommandsService() {
  queue_.RemoveObserver(this);
}

bool RemoteCommandsService::FetchRemoteCommands() {
  // TODO(hunyadym): Remove after crbug.com/582506 is fixed.
  SYSLOG(INFO) << "Fetching remote commands.";
  if (!client_->is_registered()) {
    SYSLOG(WARNING) << "Client is not registered.";
    return false;
  }

  if (command_fetch_in_progress_) {
    // TODO(hunyadym): Remove after crbug.com/582506 is fixed.
    SYSLOG(WARNING) << "Command fetch is already in progress.";
    has_enqueued_fetch_request_ = true;
    return false;
  }

  command_fetch_in_progress_ = true;
  has_enqueued_fetch_request_ = false;

  std::vector<em::RemoteCommandResult> previous_results;
  unsent_results_.swap(previous_results);

  std::unique_ptr<RemoteCommandJob::UniqueIDType> id_to_acknowledge;

  if (has_finished_command_) {
    // Acknowledges |lastest_finished_command_id_|, and removes it and every
    // command before it from |fetched_command_ids_|.
    id_to_acknowledge.reset(
        new RemoteCommandJob::UniqueIDType(lastest_finished_command_id_));
    // It's safe to remove these IDs from |fetched_command_ids_| here, since
    // it is guaranteed that there is no earlier fetch request in progress
    // anymore that could have returned these IDs.
    while (!fetched_command_ids_.empty() &&
           fetched_command_ids_.front() != lastest_finished_command_id_) {
      fetched_command_ids_.pop_front();
    }
  }

  client_->FetchRemoteCommands(
      std::move(id_to_acknowledge), previous_results,
      base::BindOnce(&RemoteCommandsService::OnRemoteCommandsFetched,
                     weak_factory_.GetWeakPtr()));

  return true;
}

void RemoteCommandsService::SetClocksForTesting(
    const base::Clock* clock,
    const base::TickClock* tick_clock) {
  queue_.SetClocksForTesting(clock, tick_clock);
}

void RemoteCommandsService::SetOnCommandAckedCallback(
    base::OnceClosure callback) {
  on_command_acked_callback_ = std::move(callback);
}

void RemoteCommandsService::VerifyAndEnqueueSignedCommand(
    const em::SignedData& signed_command) {
  const bool valid_signature = CloudPolicyValidatorBase::VerifySignature(
      signed_command.data(), store_->policy_signature_public_key(),
      signed_command.signature(),
      CloudPolicyValidatorBase::SignatureType::SHA1);

  auto ignore_result = base::BindOnce(
      [](std::vector<em::RemoteCommandResult>* unsent_results,
         const char* error_msg) {
        SYSLOG(ERROR) << error_msg;
        em::RemoteCommandResult result;
        result.set_result(em::RemoteCommandResult_ResultType_RESULT_IGNORED);
        result.set_command_id(-1);
        unsent_results->push_back(result);
      },
      &unsent_results_);

  if (!valid_signature) {
    std::move(ignore_result)
        .Run("Secure remote command signature verification failed");
    return;
  }

  em::PolicyData policy_data;
  if (!policy_data.ParseFromString(signed_command.data()) ||
      !policy_data.has_policy_type() ||
      policy_data.policy_type() !=
          dm_protocol::kChromeRemoteCommandPolicyType) {
    std::move(ignore_result)
        .Run("Secure remote command with wrong PolicyData type");
    return;
  }

  em::RemoteCommand command;
  if (!policy_data.has_policy_value() ||
      !command.ParseFromString(policy_data.policy_value())) {
    std::move(ignore_result)
        .Run("Secure remote command invalid RemoteCommand data");
    return;
  }

  const em::PolicyData* const policy = store_->policy();
  if (!policy || policy->device_id() != command.target_device_id()) {
    std::move(ignore_result)
        .Run("Secure remote command wrong target device id");
    return;
  }

  // Signature verification passed.
  EnqueueCommand(command, &signed_command);
}

void RemoteCommandsService::EnqueueCommand(
    const em::RemoteCommand& command,
    const em::SignedData* signed_command) {
  if (!command.has_type() || !command.has_command_id()) {
    SYSLOG(ERROR) << "Invalid remote command from server.";
    return;
  }

  // If the command is already fetched, ignore it.
  if (base::Contains(fetched_command_ids_, command.command_id()))
    return;

  fetched_command_ids_.push_back(command.command_id());

  std::unique_ptr<RemoteCommandJob> job =
      factory_->BuildJobForType(command.type(), this);

  if (!job || !job->Init(queue_.GetNowTicks(), command, signed_command)) {
    SYSLOG(ERROR) << "Initialization of remote command type "
                  << command.type() << " with id " << command.command_id()
                  << " failed.";
    em::RemoteCommandResult ignored_result;
    ignored_result.set_result(
        em::RemoteCommandResult_ResultType_RESULT_IGNORED);
    ignored_result.set_command_id(command.command_id());
    unsent_results_.push_back(ignored_result);
    return;
  }

  queue_.AddJob(std::move(job));
}

void RemoteCommandsService::OnJobStarted(RemoteCommandJob* command) {
}

void RemoteCommandsService::OnJobFinished(RemoteCommandJob* command) {
  has_finished_command_ = true;
  lastest_finished_command_id_ = command->unique_id();
  // TODO(binjin): Attempt to sync |lastest_finished_command_id_| to some
  // persistent source, so that we can reload it later without relying solely on
  // the server to keep our last acknowledged command ID.
  // See http://crbug.com/466572.

  em::RemoteCommandResult result;
  result.set_command_id(command->unique_id());
  result.set_timestamp(command->execution_started_time().ToJavaTime());

  if (command->status() == RemoteCommandJob::SUCCEEDED ||
      command->status() == RemoteCommandJob::FAILED) {
    if (command->status() == RemoteCommandJob::SUCCEEDED)
      result.set_result(em::RemoteCommandResult_ResultType_RESULT_SUCCESS);
    else
      result.set_result(em::RemoteCommandResult_ResultType_RESULT_FAILURE);
    const std::unique_ptr<std::string> result_payload =
        command->GetResultPayload();
    if (result_payload)
      result.set_payload(*result_payload);
  } else if (command->status() == RemoteCommandJob::EXPIRED ||
             command->status() == RemoteCommandJob::INVALID) {
    result.set_result(em::RemoteCommandResult_ResultType_RESULT_IGNORED);
  } else {
    NOTREACHED();
  }

  SYSLOG(INFO) << "Remote command " << command->unique_id()
               << " finished with result " << result.result();

  unsent_results_.push_back(result);

  FetchRemoteCommands();
}

void RemoteCommandsService::OnRemoteCommandsFetched(
    DeviceManagementStatus status,
    const std::vector<enterprise_management::RemoteCommand>& commands,
    const std::vector<enterprise_management::SignedData>& signed_commands) {
  DCHECK(command_fetch_in_progress_);
  // TODO(hunyadym): Remove after crbug.com/582506 is fixed.
  SYSLOG(INFO) << "Remote commands fetched.";
  command_fetch_in_progress_ = false;

  if (!on_command_acked_callback_.is_null())
    std::move(on_command_acked_callback_).Run();

  // TODO(binjin): Add retrying on errors. See http://crbug.com/466572.
  if (status == DM_STATUS_SUCCESS) {
    for (const auto& command : commands)
      EnqueueCommand(command, nullptr /* signed_command */);
    for (const auto& signed_command : signed_commands)
      VerifyAndEnqueueSignedCommand(signed_command);
  }

  // Start another fetch request job immediately if there are unsent command
  // results or enqueued fetch requests.
  if (!unsent_results_.empty() || has_enqueued_fetch_request_)
    FetchRemoteCommands();
}

}  // namespace policy
