// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_DM_CLIENT_H_
#define CHROME_ENTERPRISE_COMPANION_DM_CLIENT_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/enterprise_companion/device_management_storage/dm_storage.h"
#include "chrome/enterprise_companion/enterprise_companion_status.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/core/common/cloud/device_management_service.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace policy {
class CloudPolicyClient;
}  // namespace policy

namespace enterprise_companion {

class EventLogger;

extern const char kGoogleUpdateMachineLevelAppsPolicyType[];

using CloudPolicyClientProvider =
    base::OnceCallback<std::unique_ptr<policy::CloudPolicyClient>(
        policy::DeviceManagementService* dm_service)>;

// A functional interface for validating policy fetch results. Must not produce
// a nullptr.
using PolicyFetchResponseValidator = base::RepeatingCallback<
    std::unique_ptr<policy::CloudPolicyValidatorBase::ValidationResult>(
        const std::string& dm_token,
        const std::string& device_id,
        const std::string& cached_policy_public_key,
        int64_t cached_policy_timestamp,
        const enterprise_management::PolicyFetchResponse& response)>;

// Interface for performing device management functionality. Callbacks are
// responded to on the current sequence.
class DMClient {
 public:
  virtual ~DMClient() = default;

  // Register the companion app with the enrollment token from storage.
  virtual void RegisterPolicyAgent(scoped_refptr<EventLogger> event_logger,
                                   StatusCallback callback) = 0;

  // Fetch policies using the DM token from storage.
  virtual void FetchPolicies(scoped_refptr<EventLogger> event_logger,
                             StatusCallback callback) = 0;
};

CloudPolicyClientProvider GetDefaultCloudPolicyClientProvider(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory);

PolicyFetchResponseValidator GetDefaultPolicyFetchResponseValidator();

std::unique_ptr<policy::DeviceManagementService::Configuration>
CreateDeviceManagementServiceConfig();

// Creates a DMClient. |cloud_policy_client_provider| is used to construct the
// underlying CloudPolicyClient on a separate sequence.
std::unique_ptr<DMClient> CreateDMClient(
    CloudPolicyClientProvider cloud_policy_client_provider,
    scoped_refptr<device_management_storage::DMStorage> dm_storage =
        device_management_storage::GetDefaultDMStorage(),
    PolicyFetchResponseValidator policy_fetch_response_validator =
        GetDefaultPolicyFetchResponseValidator(),
    std::unique_ptr<policy::DeviceManagementService::Configuration> config =
        CreateDeviceManagementServiceConfig());

}  // namespace enterprise_companion

#endif  // CHROME_ENTERPRISE_COMPANION_DM_CLIENT_H_
