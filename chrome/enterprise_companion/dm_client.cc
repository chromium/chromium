// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/dm_client.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "chrome/enterprise_companion/device_management_storage/dm_storage.h"
#include "chrome/enterprise_companion/enterprise_companion_branding.h"
#include "chrome/enterprise_companion/enterprise_companion_status.h"
#include "chrome/enterprise_companion/enterprise_companion_version.h"
#include "chrome/enterprise_companion/event_logger.h"
#include "chrome/enterprise_companion/global_constants.h"
#include "components/policy/core/common/cloud/client_data_delegate.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace enterprise_companion {

namespace {

// Given the void-returning callbacks A and B with the same signature, return a
// callback that invokes A and B in sequence with the same arguments.
template <typename... Args>
base::OnceCallback<void(Args...)> TeeOnceCallback(
    base::OnceCallback<void(Args...)> a,
    base::OnceCallback<void(Args...)> b) {
  return base::BindOnce(
      [](base::OnceCallback<void(Args...)> a,
         base::OnceCallback<void(Args...)> b, Args... args) {
        std::move(a).Run(args...);
        std::move(b).Run(args...);
      },
      std::move(a), std::move(b));
}

// Convert a CloudPolicyClient::ResponseMap to a DMPolicyMap by dropping the
// "settings entity ID" from the key and serializing the fetch response.
device_management_storage::DMPolicyMap ToDMPolicyMap(
    const policy::CloudPolicyClient::ResponseMap& in) {
  device_management_storage::DMPolicyMap out;
  base::ranges::transform(
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
  DMConfiguration() = default;
  ~DMConfiguration() override = default;

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
  ClientDataDelegate() = default;
  ~ClientDataDelegate() override = default;

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

// Interface to a CloudPolicyClient which interacts with the device management
// server. May perform blocking IO.
class DMClientImpl : public DMClient, policy::CloudPolicyClient::Observer {
 public:
  explicit DMClientImpl(
      std::unique_ptr<policy::DeviceManagementService::Configuration> config,
      CloudPolicyClientProvider cloud_policy_client_provider,
      scoped_refptr<device_management_storage::DMStorage> dm_storage,
      PolicyFetchResponseValidator policy_fetch_response_validator)
      : dm_service_(std::move(config)),
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
      cloud_policy_client_->SetupRegistration(dm_storage_->GetDmToken(),
                                              dm_storage_->GetDeviceID(),
                                              /*user_affiliation_ids=*/{});
    }
    UpdateCachedPolicyInfo();
  }
  ~DMClientImpl() override { cloud_policy_client_->RemoveObserver(this); }

  // Overrides for DMClient.
  void RegisterPolicyAgent(scoped_refptr<EventLogger> event_logger,
                           StatusCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(!pending_callback_);

    if (ShouldSkipRegistration()) {
      std::move(callback).Run(EnterpriseCompanionStatus::Success());
      return;
    }

    dm_storage_->RemoveAllPolicies();
    pending_callback_ =
        TeeOnceCallback(std::move(callback), event_logger->OnEnrollmentStart());
    cloud_policy_client_->RegisterPolicyAgentWithEnrollmentToken(
        dm_storage_->GetEnrollmentToken(), dm_storage_->GetDeviceID(),
        client_data_delegate_);
  }

  void FetchPolicies(scoped_refptr<EventLogger> event_logger,
                     StatusCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(!pending_callback_);
    // Wrap the callback with event logging early to ensure that precondition
    // errors are logged.
    callback = TeeOnceCallback(std::move(callback),
                               event_logger->OnPolicyFetchStart());

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

    pending_callback_ = std::move(callback);
    UpdateCachedPolicyInfo();
    cloud_policy_client_->FetchPolicy(policy::PolicyFetchReason::kUnspecified);
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

    // Make a copy to reduce the surface of TOCTOU errors between validation and
    // serialization.
    policy::CloudPolicyClient::ResponseMap responses =
        cloud_policy_client_->last_policy_fetch_responses();
    FetchedPolicyValidator::Status validation_result =
        ValidatePolicyFetchResponses(responses);
    if (validation_result != FetchedPolicyValidator::VALIDATION_OK) {
      std::move(callback).Run(
          EnterpriseCompanionStatus::FromCloudPolicyValidationResult(
              validation_result));
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
  SEQUENCE_CHECKER(sequence_checker_);

  policy::DeviceManagementService dm_service_;
  std::unique_ptr<policy::CloudPolicyClient> cloud_policy_client_;
  scoped_refptr<device_management_storage::DMStorage> dm_storage_;
  PolicyFetchResponseValidator policy_fetch_response_validator_;
  ClientDataDelegate client_data_delegate_;
  StatusCallback pending_callback_;
  std::unique_ptr<device_management_storage::CachedPolicyInfo>
      cached_policy_info_;

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

  // Validates all of the fetched policies. Returns the last encountered error
  // or `VALIDATION_OK`.
  FetchedPolicyValidator::Status ValidatePolicyFetchResponses(
      const policy::CloudPolicyClient::ResponseMap& responses) {
    FetchedPolicyValidator::Status last_result =
        FetchedPolicyValidator::VALIDATION_OK;
    for (const auto& [key, response] : responses) {
      std::unique_ptr<FetchedPolicyValidator::ValidationResult>
          validation_result = policy_fetch_response_validator_.Run(
              dm_storage_->GetDmToken(), dm_storage_->GetDeviceID(),
              cached_policy_info_->public_key(),
              cached_policy_info_->timestamp(), response);
      CHECK(validation_result) << "Policy validation result cannot be null";
      if (validation_result->status != FetchedPolicyValidator::VALIDATION_OK) {
        LOG(ERROR) << "Policy validation failed for " << key.first
                   << " response: "
                   << FetchedPolicyValidator::StatusToString(
                          validation_result->status);
        last_result = validation_result->status;
      }
    }
    return last_result;
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
    std::unique_ptr<policy::DeviceManagementService::Configuration> config) {
  return std::make_unique<DMClientImpl>(
      std::move(config), std::move(cloud_policy_client_provider), dm_storage,
      policy_fetch_response_validator);
}

}  // namespace enterprise_companion
