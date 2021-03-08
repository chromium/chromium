// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/device_management/dm_message.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "chrome/updater/device_management/dm_response_validator.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace updater {

std::string GetRegisterBrowserRequestData(const std::string& machine_name,
                                          const std::string& os_platform,
                                          const std::string& os_version) {
  enterprise_management::DeviceManagementRequest dm_request;

  ::enterprise_management::RegisterBrowserRequest* request =
      dm_request.mutable_register_browser_request();
  request->set_machine_name(machine_name);
  request->set_os_platform(os_platform);
  request->set_os_version(os_version);

  return dm_request.SerializeAsString();
}

std::string GetPolicyFetchRequestData(const std::string& policy_type,
                                      const CachedPolicyInfo& policy_info) {
  enterprise_management::DeviceManagementRequest dm_request;

  enterprise_management::PolicyFetchRequest* policy_fetch_request =
      dm_request.mutable_policy_request()->add_requests();
  policy_fetch_request->set_policy_type(policy_type);
  policy_fetch_request->set_signature_type(
      enterprise_management::PolicyFetchRequest::SHA256_RSA);
  policy_fetch_request->set_verification_key_hash(
      policy::kPolicyVerificationKeyHash);

  if (policy_info.has_key_version()) {
    policy_fetch_request->set_public_key_version(policy_info.key_version());
  }

  return dm_request.SerializeAsString();
}

std::string ParseDeviceRegistrationResponse(const std::string& response_data) {
  enterprise_management::DeviceManagementResponse dm_response;
  if (!dm_response.ParseFromString(response_data) ||
      !dm_response.has_register_response() ||
      !dm_response.register_response().has_device_management_token()) {
    return std::string();
  }

  return dm_response.register_response().device_management_token();
}

DMPolicyMap ParsePolicyFetchResponse(
    const std::string& response_data,
    const CachedPolicyInfo& policy_info,
    const std::string& expected_dm_token,
    const std::string& expected_device_id,
    std::vector<PolicyValidationResult>& validation_results) {
  enterprise_management::DeviceManagementResponse dm_response;
  if (!dm_response.ParseFromString(response_data) ||
      !dm_response.has_policy_response() ||
      dm_response.policy_response().responses_size() == 0) {
    return {};
  }

  DMResponseValidator validator(policy_info, expected_dm_token,
                                expected_device_id);

  // Validate each individual policy and put valid ones into the returned policy
  // map.
  DMPolicyMap responses;
  for (int i = 0; i < dm_response.policy_response().responses_size(); ++i) {
    PolicyValidationResult validation_result;
    const ::enterprise_management::PolicyFetchResponse& response =
        dm_response.policy_response().responses(i);

    if (!validator.ValidatePolicy(response, validation_result)) {
      VLOG(1) << "Policy " << validation_result.policy_type
              << " validation failed.";
      validation_results.push_back(validation_result);
      continue;
    }

    const std::string policy_type = validation_result.policy_type;
    if (responses.find(policy_type) != responses.end()) {
      VLOG(1) << "Duplicate PolicyFetchResponse for type " << policy_type;
      continue;
    }

    std::string policy_fetch_response;
    if (!response.SerializeToString(&policy_fetch_response)) {
      VLOG(1) << "Failed to serialize policy " << policy_type;
      continue;
    }

    responses.emplace(policy_type, std::move(policy_fetch_response));
  }

  return responses;
}

}  // namespace updater
