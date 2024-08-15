// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/remote_commands/remote_commands_service.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/syslog_logging.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/policy/core/common/remote_commands/remote_commands_factory.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

namespace em = enterprise_management;

namespace {

RemoteCommandsService::MetricReceivedRemoteCommand RemoteCommandMetricFromType(
    em::RemoteCommand_Type type) {
  using Metric = RemoteCommandsService::MetricReceivedRemoteCommand;

  switch (type) {
    case em::RemoteCommand_Type_COMMAND_ECHO_TEST:
      return Metric::kCommandEchoTest;
    case em::RemoteCommand_Type_DEVICE_REBOOT:
      return Metric::kDeviceReboot;
    case em::RemoteCommand_Type_DEVICE_SCREENSHOT:
      return Metric::kDeviceScreenshot;
    case em::RemoteCommand_Type_DEVICE_SET_VOLUME:
      return Metric::kDeviceSetVolume;
    case em::RemoteCommand_Type_DEVICE_START_CRD_SESSION:
      return Metric::kDeviceStartCrdSession;
    case em::RemoteCommand_Type_DEVICE_FETCH_STATUS:
      return Metric::kDeviceFetchStatus;
    case em::RemoteCommand_Type_USER_ARC_COMMAND:
      return Metric::kUserArcCommand;
    case em::RemoteCommand_Type_DEVICE_WIPE_USERS:
      return Metric::kDeviceWipeUsers;
    case em::RemoteCommand_Type_DEVICE_REFRESH_ENTERPRISE_MACHINE_CERTIFICATE:
      return Metric::kDeviceRefreshEnterpriseMachineCertificate;
    case em::RemoteCommand_Type_DEVICE_REMOTE_POWERWASH:
      return Metric::kDeviceRemotePowerwash;
    case em::RemoteCommand_Type_DEVICE_GET_AVAILABLE_DIAGNOSTIC_ROUTINES:
      return Metric::kDeviceGetAvailableDiagnosticRoutines;
    case em::RemoteCommand_Type_DEVICE_RUN_DIAGNOSTIC_ROUTINE:
      return Metric::kDeviceRunDiagnosticRoutine;
    case em::RemoteCommand_Type_DEVICE_GET_DIAGNOSTIC_ROUTINE_UPDATE:
      return Metric::kDeviceGetDiagnosticRoutineUpdate;
    case em::RemoteCommand_Type_BROWSER_CLEAR_BROWSING_DATA:
      return Metric::kBrowserClearBrowsingData;
    case em::RemoteCommand_Type_DEVICE_RESET_EUICC:
      return Metric::kDeviceResetEuicc;
    case em::RemoteCommand_Type_BROWSER_ROTATE_ATTESTATION_CREDENTIAL:
      return Metric::kBrowserRotateAttestationCredential;
    case em::RemoteCommand_Type_FETCH_CRD_AVAILABILITY_INFO:
      return Metric::kFetchCrdAvailabilityInfo;
    case em::RemoteCommand_Type_FETCH_SUPPORT_PACKET:
      return Metric::kFetchSupportPacket;
  }

  // None of possible types matched. May indicate that there is new unhandled
  // command type.
  NOTREACHED_IN_MIGRATION() << "Unknown command type to record: " << type;
  return Metric::kUnknownType;
}

const char* RemoteCommandTypeToString(em::RemoteCommand_Type type) {
  switch (type) {
    case em::RemoteCommand_Type_COMMAND_ECHO_TEST:
      return "CommandEchoTest";
    case em::RemoteCommand_Type_DEVICE_REBOOT:
      return "DeviceReboot";
    case em::RemoteCommand_Type_DEVICE_SCREENSHOT:
      return "DeviceScreenshot";
    case em::RemoteCommand_Type_DEVICE_SET_VOLUME:
      return "DeviceSetVolume";
    case em::RemoteCommand_Type_DEVICE_START_CRD_SESSION:
      return "DeviceStartCrdSession";
    case em::RemoteCommand_Type_DEVICE_FETCH_STATUS:
      return "DeviceFetchStatus";
    case em::RemoteCommand_Type_USER_ARC_COMMAND:
      return "UserArcCommand";
    case em::RemoteCommand_Type_DEVICE_WIPE_USERS:
      return "DeviceWipeUsers";
    case em::RemoteCommand_Type_DEVICE_REFRESH_ENTERPRISE_MACHINE_CERTIFICATE:
      return "DeviceRefreshEnterpriseMachineCertificate";
    case em::RemoteCommand_Type_DEVICE_REMOTE_POWERWASH:
      return "DeviceRemotePowerwash";
    case em::RemoteCommand_Type_DEVICE_GET_AVAILABLE_DIAGNOSTIC_ROUTINES:
      return "DeviceGetAvailableDiagnosticRoutines";
    case em::RemoteCommand_Type_DEVICE_RUN_DIAGNOSTIC_ROUTINE:
      return "DeviceRunDiagnosticRoutine";
    case em::RemoteCommand_Type_DEVICE_GET_DIAGNOSTIC_ROUTINE_UPDATE:
      return "DeviceGetDiagnosticRoutineUpdate";
    case em::RemoteCommand_Type_BROWSER_CLEAR_BROWSING_DATA:
      return "BrowserClearBrowsingData";
    case em::RemoteCommand_Type_DEVICE_RESET_EUICC:
      return "DeviceResetEuicc";
    case em::RemoteCommand_Type_BROWSER_ROTATE_ATTESTATION_CREDENTIAL:
      return "BrowserRotateAttestationCredential";
    case em::RemoteCommand_Type_FETCH_CRD_AVAILABILITY_INFO:
      return "FetchCrdAvailabilityInfo";
    case em::RemoteCommand_Type_FETCH_SUPPORT_PACKET:
      return "FetchSupportPacket";
  }

  NOTREACHED_IN_MIGRATION() << "Unknown command type: " << type;
  return "";
}

std::string ToString(
    enterprise_management::RemoteCommandResult::ResultType type) {
#define CASE(_name)                                       \
  case enterprise_management::RemoteCommandResult::_name: \
    return #_name;

  switch (type) {
    CASE(RESULT_IGNORED);
    CASE(RESULT_FAILURE);
    CASE(RESULT_SUCCESS);
  }
  return base::StringPrintf("Unknown type %i", type);
#undef CASE
}

}  // namespace

// static
const char* RemoteCommandsService::GetMetricNameReceivedRemoteCommand(
    PolicyInvalidationScope scope) {
  switch (scope) {
    case PolicyInvalidationScope::kUser:
      return kMetricUserRemoteCommandReceived;
    case PolicyInvalidationScope::kDevice:
      return kMetricDeviceRemoteCommandReceived;
    case PolicyInvalidationScope::kCBCM:
      return kMetricCBCMRemoteCommandReceived;
    case PolicyInvalidationScope::kDeviceLocalAccount:
      NOTREACHED_IN_MIGRATION()
          << "Unexpected instance of remote commands service with "
             "device local account scope.";
      return "";
  }
}

// static
std::string RemoteCommandsService::GetMetricNameExecutedRemoteCommand(
    PolicyInvalidationScope scope,
    em::RemoteCommand_Type command_type) {
  const char* const command = RemoteCommandTypeToString(command_type);
  switch (scope) {
    case PolicyInvalidationScope::kUser:
      return base::StringPrintf(kMetricUserRemoteCommandExecutedTemplate,
                                command);
    case PolicyInvalidationScope::kDevice:
      return base::StringPrintf(kMetricDeviceRemoteCommandExecutedTemplate,
                                command);
    case PolicyInvalidationScope::kCBCM:
      return base::StringPrintf(kMetricCBCMRemoteCommandExecutedTemplate,
                                command);
    case PolicyInvalidationScope::kDeviceLocalAccount:
      NOTREACHED() << "Unexpected instance of remote commands service with "
                      "device local account scope.";
  }
}

// static
std::string RemoteCommandsService::GetRequestType(
    PolicyInvalidationScope scope) {
  switch (scope) {
    case PolicyInvalidationScope::kDevice:
    case PolicyInvalidationScope::kDeviceLocalAccount:
      return dm_protocol::kChromeDeviceRemoteCommandType;
    case PolicyInvalidationScope::kCBCM:
      return dm_protocol::kChromeBrowserRemoteCommandType;
    case PolicyInvalidationScope::kUser:
#if BUILDFLAG(IS_CHROMEOS_ASH)
      return dm_protocol::kChromeAshUserRemoteCommandType;
#else
      return dm_protocol::kChromeUserRemoteCommandType;
#endif
  }
}

RemoteCommandsService::RemoteCommandsService(
    std::unique_ptr<RemoteCommandsFactory> factory,
    CloudPolicyClient* client,
    CloudPolicyStore* store,
    PolicyInvalidationScope scope)
    : factory_(std::move(factory)),
      client_(client),
      store_(store),
      scope_(scope) {
  DCHECK(client_);
  remote_commands_queue_observation.Observe(&queue_);
  if (!factory_) {
    CHECK_IS_TEST();
  }
}

RemoteCommandsService::~RemoteCommandsService() = default;

bool RemoteCommandsService::FetchRemoteCommands() {
  if (!client_->is_registered()) {
    SYSLOG(WARNING) << "Client is not registered.";
    return false;
  }

  if (command_fetch_in_progress_) {
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
    id_to_acknowledge = std::make_unique<RemoteCommandJob::UniqueIDType>(
        lastest_finished_command_id_);
    // It's safe to remove these IDs from |fetched_command_ids_| here, since
    // it is guaranteed that there is no earlier fetch request in progress
    // anymore that could have returned these IDs.
    while (!fetched_command_ids_.empty() &&
           fetched_command_ids_.front() != lastest_finished_command_id_) {
      fetched_command_ids_.pop_front();
    }
  }

  client_->FetchRemoteCommands(
      std::move(id_to_acknowledge), previous_results, GetSignatureType(),
      GetRequestType(scope_),
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
      signed_command.signature(), GetSignatureType());

  auto ignore_result = base::BindOnce(
      [](RemoteCommandsService* self, const char* error_msg,
         MetricReceivedRemoteCommand metric) {
        SYSLOG(ERROR) << error_msg;
        em::RemoteCommandResult result;
        result.set_result(em::RemoteCommandResult_ResultType_RESULT_IGNORED);
        result.set_command_id(-1);
        self->unsent_results_.push_back(result);
        self->RecordReceivedRemoteCommand(metric);
        // Trigger another fetch so the results are uploaded.
        self->FetchRemoteCommands();
      },
      base::Unretained(this));

  if (!valid_signature) {
    std::move(ignore_result)
        .Run("Secure remote command signature verification failed",
             MetricReceivedRemoteCommand::kInvalidSignature);
    return;
  }

  em::PolicyData policy_data;
  if (!policy_data.ParseFromString(signed_command.data()) ||
      !policy_data.has_policy_type() ||
      policy_data.policy_type() !=
          dm_protocol::kChromeRemoteCommandPolicyType) {
    std::move(ignore_result)
        .Run("Secure remote command with wrong PolicyData type",
             MetricReceivedRemoteCommand::kInvalid);
    return;
  }

  em::RemoteCommand command;
  if (!policy_data.has_policy_value() ||
      !command.ParseFromString(policy_data.policy_value())) {
    std::move(ignore_result)
        .Run("Secure remote command invalid RemoteCommand data",
             MetricReceivedRemoteCommand::kInvalid);
    return;
  }

  const em::PolicyData* const policy = store_->policy();
  if (!policy || policy->device_id() != command.target_device_id()) {
    std::move(ignore_result)
        .Run("Secure remote command wrong target device id",
             MetricReceivedRemoteCommand::kInvalid);
    return;
  }

  // Signature verification passed.
  EnqueueCommand(command, signed_command);
}

void RemoteCommandsService::EnqueueCommand(
    const em::RemoteCommand& command,
    const em::SignedData& signed_command) {
  if (!command.has_type() || !command.has_command_id()) {
    SYSLOG(ERROR) << "Invalid remote command from server.";
    const auto metric = !command.has_command_id()
                            ? MetricReceivedRemoteCommand::kInvalid
                            : MetricReceivedRemoteCommand::kUnknownType;
    RecordReceivedRemoteCommand(metric);
    return;
  }

  // If the command is already fetched, ignore it.
  if (base::Contains(fetched_command_ids_, command.command_id())) {
    RecordReceivedRemoteCommand(MetricReceivedRemoteCommand::kDuplicated);
    return;
  }

  fetched_command_ids_.push_back(command.command_id());

  std::unique_ptr<RemoteCommandJob> job =
      factory_->BuildJobForType(command.type(), this);

  if (!job || !job->Init(queue_.GetNowTicks(), command, signed_command)) {
    SYSLOG(ERROR) << "Initialization of remote command type " << command.type()
                  << " with id " << command.command_id() << " failed.";
    const auto metric = job == nullptr
                            ? MetricReceivedRemoteCommand::kInvalidScope
                            : MetricReceivedRemoteCommand::kInvalid;
    RecordReceivedRemoteCommand(metric);
    em::RemoteCommandResult ignored_result;
    ignored_result.set_result(
        em::RemoteCommandResult_ResultType_RESULT_IGNORED);
    ignored_result.set_command_id(command.command_id());
    unsent_results_.push_back(ignored_result);
    return;
  }

  RecordReceivedRemoteCommand(RemoteCommandMetricFromType(command.type()));

  queue_.AddJob(std::move(job));
}

void RemoteCommandsService::OnJobStarted(RemoteCommandJob* command) {}

void RemoteCommandsService::OnJobFinished(RemoteCommandJob* command) {
  has_finished_command_ = true;
  lastest_finished_command_id_ = command->unique_id();
  // TODO(binjin): Attempt to sync |lastest_finished_command_id_| to some
  // persistent source, so that we can reload it later without relying solely on
  // the server to keep our last acknowledged command ID.
  // See http://crbug.com/466572.

  if (command->GetResult()) {
    em::RemoteCommandResult result;
    result.set_command_id(command->unique_id());
    result.set_timestamp(
        command->execution_started_time().InMillisecondsSinceUnixEpoch());
    result.set_result(command->GetResult().value());

    std::unique_ptr<std::string> result_payload = command->GetResultPayload();
    if (result_payload) {
      result.set_payload(std::move(*result_payload));
    }

    SYSLOG(INFO) << "Remote command " << command->unique_id()
                 << " finished with result " << ToString(result.result())
                 << " (" << result.result() << ")";

    unsent_results_.push_back(result);
  }

  RecordExecutedRemoteCommand(*command);

  FetchRemoteCommands();
}

void RemoteCommandsService::OnRemoteCommandsFetched(
    DeviceManagementStatus status,
    const std::vector<enterprise_management::SignedData>& commands) {
  DCHECK(command_fetch_in_progress_);
  command_fetch_in_progress_ = false;

  if (!on_command_acked_callback_.is_null()) {
    std::move(on_command_acked_callback_).Run();
  }

  // TODO(binjin): Add retrying on errors. See http://crbug.com/466572.
  if (status == DM_STATUS_SUCCESS) {
    for (const auto& command : commands) {
      VerifyAndEnqueueSignedCommand(command);
    }
  }

  // Start another fetch request job immediately if there are unsent command
  // results or enqueued fetch requests.
  if (!unsent_results_.empty() || has_enqueued_fetch_request_) {
    FetchRemoteCommands();
  }
}

void RemoteCommandsService::RecordReceivedRemoteCommand(
    RemoteCommandsService::MetricReceivedRemoteCommand metric) const {
  const char* metric_name = GetMetricNameReceivedRemoteCommand(scope_);
  base::UmaHistogramEnumeration(metric_name, metric);
}

void RemoteCommandsService::RecordExecutedRemoteCommand(
    const RemoteCommandJob& command) const {
  const std::string metric_name =
      GetMetricNameExecutedRemoteCommand(scope_, command.GetType());
  base::UmaHistogramEnumeration(metric_name, command.status(),
                                RemoteCommandJob::STATUS_TYPE_SIZE);
}

}  // namespace policy
