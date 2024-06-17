// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_DEVICE_MANAGEMENT_DM_MESSAGE_H_
#define CHROME_UPDATER_DEVICE_MANAGEMENT_DM_MESSAGE_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "chrome/enterprise_companion/device_management_storage/dm_storage.h"

namespace updater {

class CachedPolicyInfo;
struct PolicyValidationResult;

// DM policy map: policy_type --> serialized policy data of PolicyFetchResponse.
using DMPolicyMap = base::flat_map<std::string, std::string>;

// The policy type for Omaha policy settings.
extern const char kGoogleUpdatePolicyType[];

// Returns the serialized data from a DeviceManagementRequest, which wraps
// a RegisterBrowserRequest, to register the current device.
std::string GetRegisterBrowserRequestData();

// Returns the serialized data from a DeviceManagementRequest, which wraps
// a PolicyFetchRequest, to fetch policies for the given type.
std::string GetPolicyFetchRequestData(
    const std::string& policy_type,
    const device_management_storage::CachedPolicyInfo& policy_info);

// Returns the serialized data from a DeviceManagementRequest, which wraps
// a PolicyValidationReportRequest, to report possible policy validation errors.
std::string GetPolicyValidationReportRequestData(
    const PolicyValidationResult& validation_result);

// Parses the DeviceManagementResponse for a device registration request, and
// returns the DM token. Returns empty string if parsing failed or the response
// is unexpected.
std::string ParseDeviceRegistrationResponse(const std::string& response_data);

// Determines whether the DMToken is expected to be deleted based on the
// DMServer response contents.
bool ShouldDeleteDmToken(const std::string& response_data);

// Parses and validates the DeviceManagementResponse for a policy fetch request,
// and returns valid policies in the DMPolicyMap. All validation issues will be
// put into the `validation_results`, if there's any. `policy_info`,
// `expected_dm_token`, `expected_device_id` are used for validation, to check
// response's the signatures and whether it is intended for current device.
DMPolicyMap ParsePolicyFetchResponse(
    const std::string& response_data,
    const device_management_storage::CachedPolicyInfo& policy_info,
    const std::string& expected_dm_token,
    const std::string& expected_device_id,
    std::vector<PolicyValidationResult>& validation_results);

}  // namespace updater

#endif  // CHROME_UPDATER_DEVICE_MANAGEMENT_DM_MESSAGE_H_
