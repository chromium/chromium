// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/dm_client.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <ostream>
#include <queue>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/enterprise_companion/constants.h"
#include "chrome/enterprise_companion/device_management_storage/dm_storage.h"
#include "chrome/enterprise_companion/enterprise_companion_branding.h"
#include "chrome/enterprise_companion/enterprise_companion_status.h"
#include "chrome/enterprise_companion/enterprise_companion_version.h"
#include "chrome/enterprise_companion/event_logger.h"
#include "chrome/enterprise_companion/global_constants.h"
#include "chrome/enterprise_companion/proto/enterprise_companion_event.pb.h"
#include "chrome/enterprise_companion/telemetry_logger/telemetry_logger.h"
#include "components/policy/core/common/cloud/client_data_delegate.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/policy_value_validator.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "net/base/net_errors.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace enterprise_companion {

namespace {

std::ostream& operator<<(std::ostream& os,
                         const policy::CloudPolicyClient::Result& result) {
  if (result.IsSuccess()) {
    os << "Success";
  } else if (result.IsClientNotRegisteredError()) {
    os << "Client not registered";
  } else if (result.IsDMServerError()) {
    os << EnterpriseCompanionStatus::FromDeviceManagementStatus(
              result.GetDMServerError())
              .description();
  } else if (result.GetNetError() != net::OK) {
    os << "Network error: " << net::ErrorToString(result.GetNetError());
  }
  return os;
}

// Convert a CloudPolicyClient::ResponseMap to a DMPolicyMap by dropping the
// "settings entity ID" from the key and serializing the fetch response.
device_management_storage::DMPolicyMap ToDMPolicyMap(
    const policy::CloudPolicyClient::ResponseMap& in) {
  device_management_storage::DMPolicyMap out;
  std::ranges::transform(
      in, std::inserter(out, out.end()),
      [](const std::pair<std::pair<std::string, std::string>,
                         enterprise_management::PolicyFetchResponse> response) {
        return std::make_pair(response.first.first,
                              response.second.SerializeAsString());
      });
  return out;
}

class DMConfiguration : public policy::DeviceManagementService::Configuration {
 public:
  std::string GetDMServerUrl() const override {
    return GetGlobalConstants()->DeviceManagementServerURL().spec();
  }
  std::string GetAgentParameter() const override {
    return base::StrCat(
        {PRODUCT_FULLNAME_STRING, " ", kEnterpriseCompanionVersion});
  }
  std::string GetPlatformParameter() const override {
    int32_t major = 0;
    int32_t minor = 0;
    int32_t bugfix = 0;
    base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);
    return base::StringPrintf(
        "%s|%s|%d.%d.%d", base::SysInfo::OperatingSystemName().c_str(),
        base::SysInfo::OperatingSystemArchitecture().c_str(), major, minor,
        bugfix);
  }
  std::string GetRealtimeReportingServerUrl() const override {
    return GetGlobalConstants()->DeviceManagementRealtimeReportingURL().spec();
  }
  std::string GetEncryptedReportingServerUrl() const override {
    return GetGlobalConstants()->DeviceManagementEncryptedReportingURL().spec();
  }
};

class ClientDataDelegate : public policy::ClientDataDelegate {
 public:
  void FillRegisterBrowserRequest(
      enterprise_management::RegisterBrowserRequest* request,
      base::OnceClosure callback) const override {
    request->set_machine_name(policy::GetMachineName());
    request->set_os_platform(policy::GetOSPlatform());
    request->set_os_version(policy::GetOSVersion());
    request->set_allocated_browser_device_identifier(
        policy::GetBrowserDeviceIdentifier().release());
    std::move(callback).Run();
  }
};

class FetchedPolicyValidator final : public policy::CloudPolicyValidatorBase {
 public:
  explicit FetchedPolicyValidator(
      std::unique_ptr<enterprise_management::PolicyFetchResponse>
          policy_response)
      : policy::CloudPolicyValidatorBase(std::move(policy_response),
                                         /*background_task_runner=*/nullptr) {}
  FetchedPolicyValidator(const FetchedPolicyValidator&) = delete;
  FetchedPolicyValidator& operator=(const FetchedPolicyValidator&) = delete;

 private:
  // Overrides for CloudPolicyValidatorBase.
  Status CheckPayload() override {
    // The payload is valid so long as at least one policy is present.
    return (policy_data() && policy_data()->has_policy_value())
               ? VALIDATION_OK
               : VALIDATION_POLICY_PARSE_ERROR;
  }

  Status CheckValues() override {
    // The enterprise companion is agnostic to the type of payload. Hence, the
    // values are not verified.
    return VALIDATION_OK;
  }
};

// `BufferedDMClient` sequences calls to another DMClient ensuring that no
// operations overlap. This is useful as the CloudPolicyClient backing
// DMClientImpl does not provide any means to discriminate the origin of
// overlapping calls nor does it make any guarantees of the behavior of such
// calls.
class BufferedDMClient : public DMClient {
 public:
  explicit BufferedDMClient(std::unique_ptr<DMClient> client)
      : client_(std::move(client)) {}

  ~BufferedDMClient() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    VLOG_IF(1, !tasks_.empty())
        << "BufferedDMClient destroyed while " << tasks_.size()
        << " tasks were pending. Callbacks will be dropped.";
    VLOG_IF(1, task_running_) << "BufferedDMClient destroyed while a task was "
                                 "in-flight; its callback will be dropped.";
  }

  void RegisterPolicyAgent(scoped_refptr<EnterpriseCompanionEventLogger> logger,
                           StatusCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    Enqueue(base::BindOnce(&BufferedDMClient::DoRegisterPolicyAgent,
                           weak_ptr_factory_.GetWeakPtr(), logger,
                           std::move(callback)));
  }

  void FetchPolicies(policy::PolicyFetchReason reason,
                     scoped_refptr<EnterpriseCompanionEventLogger> logger,
                     StatusCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    Enqueue(base::BindOnce(&BufferedDMClient::DoFetchPolicies,
                           weak_ptr_factory_.GetWeakPtr(), reason, logger,
                           std::move(callback)));
  }

 private:
  void DoRegisterPolicyAgent(
      scoped_refptr<EnterpriseCompanionEventLogger> logger,
      StatusCallback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    client_->RegisterPolicyAgent(
        logger, base::BindOnce(&BufferedDMClient::OnTaskComplete,
                               base::Unretained(this), std::move(callback)));
  }

  void DoFetchPolicies(policy::PolicyFetchReason reason,
                       scoped_refptr<EnterpriseCompanionEventLogger> logger,
                       StatusCallback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    client_->FetchPolicies(
        reason, logger,
        base::BindOnce(&BufferedDMClient::OnTaskComplete,
                       base::Unretained(this), std::move(callback)));
  }

  void OnTaskComplete(StatusCallback callback,
                      const EnterpriseCompanionStatus& status) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), status));
    task_running_ = false;
    ScheduleNextTask();
  }

  // Adds a task to the queue. Tasks should reference the BufferedDMClient by
  // weak_ptr as they are posted.
  void Enqueue(base::OnceClosure task) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    tasks_.push(std::move(task));
    ScheduleNextTask();
  }

  void ScheduleNextTask() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (task_running_ || tasks_.empty()) {
      return;
    }
    task_running_ = true;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(tasks_.front()));
    tasks_.pop();
  }

  SEQUENCE_CHECKER(sequence_checker_);
  std::unique_ptr<DMClient> client_;
  std::queue<base::OnceClosure> tasks_;
  bool task_running_ = false;
  base::WeakPtrFactory<BufferedDMClient> weak_ptr_factory_{this};
};

// Interface to a CloudPolicyClient which interacts with the device management
// server. May perform blocking IO.
class DMClientImpl : public DMClient, policy::CloudPolicyClient::Observer {
 public:
  explicit DMClientImpl(
      std::unique_ptr<policy::DeviceManagementService::Configuration> config,
      CloudPolicyClientProvider cloud_policy_client_provider,
      scoped_refptr<device_management_storage::DMStorage> dm_storage,
      PolicyFetchResponseValidator policy_fetch_response_validator,
      base::TimeDelta task_timeout)
      : task_timeout_(task_timeout),
        dm_service_(std::move(config)),
        cloud_policy_client_(
            std::move(cloud_policy_client_provider).Run(&dm_service_)),
        dm_storage_(dm_storage),
        policy_fetch_response_validator_(policy_fetch_response_validator) {
    dm_service_.ScheduleInitialization(0);
    cloud_policy_client_->AddObserver(this);
    cloud_policy_client_->AddPolicyTypeToFetch(
        policy::dm_protocol::kGoogleUpdateMachineLevelAppsPolicyType,
        /*settings_entity_id=*/"");
    if (!dm_storage->GetDmToken().empty()) {
      if (net::HttpUtil::IsValidHeaderValue(dm_storage->GetDmToken())) {
        cloud_policy_client_->SetupRegistration(dm_storage_->GetDmToken(),
                                                dm_storage_->GetDeviceID(),
                                                /*user_affiliation_ids=*/{});
      } else {
        VLOG(1) << "The stored DM token is malformed. The device will be "
                   "considered not registered.";
      }
    }
    UpdateCachedPolicyInfo();
  }

  ~DMClientImpl() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    cloud_policy_client_->RemoveObserver(this);
    VLOG_IF(1, pending_callback_)
        << "DMClient destroyed while task in-flight. Callback will be dropped";
  }

  // Overrides for DMClient.
  void RegisterPolicyAgent(
      scoped_refptr<EnterpriseCompanionEventLogger> event_logger,
      StatusCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(!pending_callback_) << "DMClientImpl calls may not overlap";

    if (ShouldSkipRegistration()) {
      std::move(callback).Run(EnterpriseCompanionStatus::Success());
      return;
    }

    // Wrap the callback with event logging to ensure that precondition errors
    // are logged.
    callback = base::BindPostTaskToCurrentDefault(base::BindOnce(
        &EnterpriseCompanionEventLogger::LogRegisterPolicyAgentEvent,
        event_logger, base::Time::Now(), std::move(callback)));

    if (!net::HttpUtil::IsValidHeaderValue(dm_storage_->GetEnrollmentToken())) {
      VLOG(1) << "The stored enrollment token is malformed.";
      std::move(callback).Run(
          EnterpriseCompanionStatus(ApplicationError::kInvalidEnrollmentToken));
      return;
    }

    dm_storage_->RemoveAllPolicies();
    SetPendingCallback(std::move(callback));
    cloud_policy_client_->RegisterPolicyAgentWithEnrollmentToken(
        dm_storage_->GetEnrollmentToken(), dm_storage_->GetDeviceID(),
        client_data_delegate_);
  }

  void FetchPolicies(policy::PolicyFetchReason reason,
                     scoped_refptr<EnterpriseCompanionEventLogger> event_logger,
                     StatusCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(!pending_callback_) << "DMClientImpl calls may not overlap";
    // Wrap the callback with event logging early to ensure that precondition
    // errors are logged.
    callback = base::BindPostTaskToCurrentDefault(
        base::BindOnce(&EnterpriseCompanionEventLogger::LogPolicyFetchEvent,
                       event_logger, base::Time::Now(), std::move(callback)));

    if (!cloud_policy_client_->is_registered()) {
      VLOG(1) << "Failed to fetch policies: client is not registered";
      std::move(callback).Run(EnterpriseCompanionStatus(
          ApplicationError::kRegistrationPreconditionFailed));
      return;
    }

    if (!dm_storage_->CanPersistPolicies()) {
      VLOG(1) << "Failed to fetch policies: policies cannot be persisted.";
      std::move(callback).Run(EnterpriseCompanionStatus(
          ApplicationError::kPolicyPersistenceImpossible));
      return;
    }

    SetPendingCallback(std::move(callback));
    UpdateCachedPolicyInfo();
    cloud_policy_client_->FetchPolicy(reason);
  }

  // Overrides for policy::CloudPolicyClient::Observer.
  void OnPolicyFetched(policy::CloudPolicyClient*) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    VLOG(1) << __func__;

    StatusCallback callback =
        pending_callback_ ? std::move(pending_callback_) : base::DoNothing();

    if (cloud_policy_client_->last_dm_status() !=
        policy::DeviceManagementStatus::DM_STATUS_SUCCESS) {
      std::move(callback).Run(
          EnterpriseCompanionStatus::FromDeviceManagementStatus(
              cloud_policy_client_->last_dm_status()));
      return;
    }

    policy::CloudPolicyClient::ResponseMap responses =
        cloud_policy_client_->last_policy_fetch_responses();
    FetchedPolicyValidator::ValidationResult validation_result =
        ValidatePolicyFetchResponses(responses);
    if (validation_result.status != FetchedPolicyValidator::VALIDATION_OK) {
      cloud_policy_client_->UploadPolicyValidationReport(
          validation_result.status, validation_result.value_validation_issues,
          policy::ValidationAction::kStore,
          policy::dm_protocol::kGoogleUpdateMachineLevelAppsPolicyType,
          validation_result.policy_token,
          base::BindOnce(
              [](policy::CloudPolicyClient::Result upload_report_result) {
                VLOG_IF(1, !upload_report_result.IsSuccess())
                    << "Failed to upload policy validation report: "
                    << upload_report_result;
              })
              .Then(base::BindOnce(
                  std::move(callback),
                  EnterpriseCompanionStatus::FromCloudPolicyValidationResult(
                      validation_result.status))));
      return;
    }

    if (!dm_storage_->PersistPolicies(ToDMPolicyMap(std::move(responses)))) {
      std::move(callback).Run(EnterpriseCompanionStatus(
          ApplicationError::kPolicyPersistenceFailed));
      return;
    }

    std::move(callback).Run(EnterpriseCompanionStatus::Success());
  }

  void OnRegistrationStateChanged(policy::CloudPolicyClient*) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    VLOG(1) << __func__;
    if (cloud_policy_client_->is_registered()) {
      dm_storage_->StoreDmToken(cloud_policy_client_->dm_token());
    }
    if (pending_callback_) {
      std::move(pending_callback_)
          .Run(EnterpriseCompanionStatus::FromDeviceManagementStatus(
              cloud_policy_client_->last_dm_status()));
    }
  }

  void OnClientError(policy::CloudPolicyClient*) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    VLOG(1) << __func__;
    if (cloud_policy_client_->last_dm_status() ==
        policy::DM_STATUS_SERVICE_DEVICE_NEEDS_RESET) {
      VLOG(1) << "DMServer requests deregister via DMToken deletion.";
      LOG_IF(ERROR, !dm_storage_->DeleteDMToken())
          << "Could not deregister: Failed to delete the DMToken.";
    } else if (cloud_policy_client_->last_dm_status() ==
               policy::DM_STATUS_SERVICE_DEVICE_NOT_FOUND) {
      VLOG(1) << "DMServer requests deregister via DMToken invalidation.";
      LOG_IF(ERROR, !dm_storage_->InvalidateDMToken())
          << "Could not deregister: Failed to invalidate the DMToken.";
    }
    if (pending_callback_) {
      std::move(pending_callback_)
          .Run(EnterpriseCompanionStatus::FromDeviceManagementStatus(
              cloud_policy_client_->last_dm_status()));
    }
  }

 private:
  bool ShouldSkipRegistration() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (dm_storage_->GetEnrollmentToken().empty()) {
      VLOG(1) << "Registration skipped: device not managed.";
      return true;
    } else if (cloud_policy_client_->is_registered()) {
      VLOG(1) << "Registration skipped: device already registered.";
      return true;
    }
    return false;
  }

  // Validates all of the fetched policies.
  FetchedPolicyValidator::ValidationResult ValidatePolicyFetchResponses(
      const policy::CloudPolicyClient::ResponseMap& responses) {
    for (const auto& [key, response] : responses) {
      std::unique_ptr<FetchedPolicyValidator::ValidationResult>
          validation_result = policy_fetch_response_validator_.Run(
              dm_storage_->GetDmToken(), dm_storage_->GetDeviceID(),
              cached_policy_info_->public_key(),
              cached_policy_info_->timestamp(), response);
      CHECK(validation_result) << "Policy validation result cannot be null";
      if (validation_result->status != FetchedPolicyValidator::VALIDATION_OK) {
        VLOG(1) << "Policy validation failed for " << key.first << " response: "
                << FetchedPolicyValidator::StatusToString(
                       validation_result->status);
        return *validation_result;
      }
    }
    return FetchedPolicyValidator::ValidationResult();
  }

  // Update the cached policy information, configuring the CloudPolicyClient
  // with the results.
  void UpdateCachedPolicyInfo() {
    cached_policy_info_ = dm_storage_->GetCachedPolicyInfo();
    if (cached_policy_info_->has_key_version()) {
      cloud_policy_client_->set_public_key_version(
          cached_policy_info_->key_version());
    }
  }

  void HandleTaskTimeout() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // If the callback has already been responded to, the task did not time out.
    if (pending_callback_) {
      VLOG(1) << "DMClient task timed out before CloudPolicyClient response";
      std::move(pending_callback_)
          .Run(EnterpriseCompanionStatus(
              ApplicationError::kCloudPolicyClientTimeout));
    }
  }

  void SetPendingCallback(StatusCallback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    task_timer_.Start(FROM_HERE, task_timeout_,
                      base::BindOnce(&DMClientImpl::HandleTaskTimeout,
                                     base::Unretained(this)));
    pending_callback_ = std::move(callback);
  }

  SEQUENCE_CHECKER(sequence_checker_);
  const base::TimeDelta task_timeout_;
  policy::DeviceManagementService dm_service_;
  std::unique_ptr<policy::CloudPolicyClient> cloud_policy_client_;
  scoped_refptr<device_management_storage::DMStorage> dm_storage_;
  PolicyFetchResponseValidator policy_fetch_response_validator_;
  ClientDataDelegate client_data_delegate_;
  StatusCallback pending_callback_;
  base::OneShotTimer task_timer_;
  std::unique_ptr<device_management_storage::CachedPolicyInfo>
      cached_policy_info_;
};

}  // namespace

CloudPolicyClientProvider GetDefaultCloudPolicyClientProvider(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory) {
  return base::BindOnce(
      [](scoped_refptr<network::SharedURLLoaderFactory>
             shared_url_loader_factory,
         policy::DeviceManagementService* dm_service) {
        return std::make_unique<policy::CloudPolicyClient>(
            dm_service, shared_url_loader_factory);
      },
      std::move(shared_url_loader_factory));
}

PolicyFetchResponseValidator GetDefaultPolicyFetchResponseValidator() {
  return base::BindRepeating([](const std::string& dm_token,
                                const std::string& device_id,
                                const std::string& cached_policy_public_key,
                                int64_t cached_policy_timestamp,
                                const enterprise_management::
                                    PolicyFetchResponse& response) {
    FetchedPolicyValidator validator(
        std::make_unique<enterprise_management::PolicyFetchResponse>(response));
    validator.ValidateDMToken(
        dm_token,
        FetchedPolicyValidator::ValidateDMTokenOption::DM_TOKEN_REQUIRED);
    validator.ValidateDeviceId(
        device_id,
        FetchedPolicyValidator::ValidateDeviceIdOption::DEVICE_ID_REQUIRED);
    validator.ValidateTimestamp(
        base::Time::FromMillisecondsSinceUnixEpoch(cached_policy_timestamp),
        FetchedPolicyValidator::ValidateTimestampOption::TIMESTAMP_VALIDATED);
    if (cached_policy_public_key.empty()) {
      validator.ValidateInitialKey("");
    } else {
      validator.ValidateSignatureAllowingRotation(cached_policy_public_key, "");
    }
    validator.ValidatePayload();
    validator.RunValidation();
    return validator.GetValidationResult();
  });
}

std::unique_ptr<policy::DeviceManagementService::Configuration>
CreateDeviceManagementServiceConfig() {
  return std::make_unique<DMConfiguration>();
}

std::unique_ptr<DMClient> CreateDMClient(
    CloudPolicyClientProvider cloud_policy_client_provider,
    scoped_refptr<device_management_storage::DMStorage> dm_storage,
    PolicyFetchResponseValidator policy_fetch_response_validator,
    std::unique_ptr<policy::DeviceManagementService::Configuration> config,
    base::TimeDelta task_timeout) {
  return std::make_unique<BufferedDMClient>(std::make_unique<DMClientImpl>(
      std::move(config), std::move(cloud_policy_client_provider), dm_storage,
      policy_fetch_response_validator, task_timeout));
}

}  // namespace enterprise_companion
