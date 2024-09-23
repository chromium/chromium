// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/device_management/dm_message.h"

#include <memory>
#include <utility>

#include "base/containers/fixed_flat_map.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "chrome/enterprise_companion/device_management_storage/dm_storage.h"
#include "chrome/updater/device_management/dm_response_validator.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace updater {

constexpr char kGoogleUpdatePolicyType[] = "google/machine-level-omaha";

namespace {

enterprise_management::PolicyValidationReportRequest::ValidationResultType
TranslatePolicyValidationResultStatus(PolicyValidationResult::Status status) {
  using Report = enterprise_management::PolicyValidationReportRequest;
  static constexpr auto kValidationStatusMap = base::MakeFixedFlatMap<
      PolicyValidationResult::Status, Report::ValidationResultType>({
      {PolicyValidationResult::Status::kValidationOK,
       Report::VALIDATION_RESULT_TYPE_SUCCESS},
      {PolicyValidationResult::Status::kValidationBadInitialSignature,
       Report::VALIDATION_RESULT_TYPE_BAD_INITIAL_SIGNATURE},
      {PolicyValidationResult::Status::kValidationBadSignature,
       Report::VALIDATION_RESULT_TYPE_BAD_SIGNATURE},
      {PolicyValidationResult::Status::kValidationErrorCodePresent,
       Report::VALIDATION_RESULT_TYPE_ERROR_CODE_PRESENT},
      {PolicyValidationResult::Status::kValidationPayloadParseError,
       Report::VALIDATION_RESULT_TYPE_PAYLOAD_PARSE_ERROR},
      {PolicyValidationResult::Status::kValidationWrongPolicyType,
       Report::VALIDATION_RESULT_TYPE_WRONG_POLICY_TYPE},
      {PolicyValidationResult::Status::kValidationWrongSettingsEntityID,
       Report::VALIDATION_RESULT_TYPE_WRONG_SETTINGS_ENTITY_ID},
      {PolicyValidationResult::Status::kValidationBadTimestamp,
       Report::VALIDATION_RESULT_TYPE_BAD_TIMESTAMP},
      {PolicyValidationResult::Status::kValidationBadDMToken,
       Report::VALIDATION_RESULT_TYPE_BAD_DM_TOKEN},
      {PolicyValidationResult::Status::kValidationBadDeviceID,
       Report::VALIDATION_RESULT_TYPE_BAD_DEVICE_ID},
      {PolicyValidationResult::Status::kValidationBadUser,
       Report::VALIDATION_RESULT_TYPE_BAD_USER},
      {PolicyValidationResult::Status::kValidationPolicyParseError,
       Report::VALIDATION_RESULT_TYPE_POLICY_PARSE_ERROR},
      {PolicyValidationResult::Status::kValidationBadKeyVerificationSignature,
       Report::VALIDATION_RESULT_TYPE_BAD_KEY_VERIFICATION_SIGNATURE},
      {PolicyValidationResult::Status::kValidationValueWarning,
       Report::VALIDATION_RESULT_TYPE_VALUE_WARNING},
      {PolicyValidationResult::Status::kValidationValueError,
       Report::VALIDATION_RESULT_TYPE_VALUE_ERROR},
  });

  const auto mapped_status = kValidationStatusMap.find(status);
  return mapped_status == kValidationStatusMap.end()
             ? Report::VALIDATION_RESULT_TYPE_ERROR_UNSPECIFIED
             : mapped_status->second;
}

enterprise_management::PolicyValueValidationIssue::ValueValidationIssueSeverity
TranslatePolicyValidationResultSeverity(
    PolicyValueValidationIssue::Severity severity) {
  using Issue = enterprise_management::PolicyValueValidationIssue;
  switch (severity) {
    case PolicyValueValidationIssue::Severity::kWarning:
      return Issue::VALUE_VALIDATION_ISSUE_SEVERITY_WARNING;
    case PolicyValueValidationIssue::Severity::kError:
      return Issue::VALUE_VALIDATION_ISSUE_SEVERITY_ERROR;
  }
}

}  // namespace

std::string GetRegisterBrowserRequestData() {
  enterprise_management::DeviceManagementRequest dm_request;

  enterprise_management::RegisterBrowserRequest* request =
      dm_request.mutable_register_browser_request();
  request->set_machine_name(policy::GetMachineName());
  request->set_os_platform(policy::GetOSPlatform());
  request->set_os_version(policy::GetOSVersion());
  request->set_allocated_browser_device_identifier(
      policy::GetBrowserDeviceIdentifier().release());

  return dm_request.SerializeAsString();
}

std::string GetPolicyFetchRequestData(
    const std::string& policy_type,
    const device_management_storage::CachedPolicyInfo& policy_info) {
  enterprise_management::DeviceManagementRequest dm_request;
  enterprise_management::DevicePolicyRequest* device_policy_request =
      dm_request.mutable_policy_request();
  device_policy_request->set_reason(
      enterprise_management::DevicePolicyRequest::SCHEDULED);

  enterprise_management::PolicyFetchRequest* policy_fetch_request =
      device_policy_request->add_requests();
  policy_fetch_request->set_policy_type(policy_type);
  policy_fetch_request->set_signature_type(
      enterprise_management::PolicyFetchRequest::SHA256_RSA);
  policy_fetch_request->set_verification_key_hash(
      policy::kPolicyVerificationKeyHash);
  policy_fetch_request->set_allocated_browser_device_identifier(
      policy::GetBrowserDeviceIdentifier().release());

  if (policy_info.has_key_version()) {
    policy_fetch_request->set_public_key_version(policy_info.key_version());
  }

  return dm_request.SerializeAsString();
}

std::string GetPolicyValidationReportRequestData(
    const PolicyValidationResult& validation_result) {
  PolicyValidationResult::Status aggregated_status = validation_result.status;

  if (aggregated_status == PolicyValidationResult::Status::kValidationOK) {
    for (const PolicyValueValidationIssue& issue : validation_result.issues) {
      switch (issue.severity) {
        case PolicyValueValidationIssue::Severity::kError:
          aggregated_status =
              PolicyValidationResult::Status::kValidationValueError;
          break;

        case PolicyValueValidationIssue::Severity::kWarning:
          aggregated_status =
              PolicyValidationResult::Status::kValidationValueWarning;
          break;
      }
    }
  }

  if (aggregated_status == PolicyValidationResult::Status::kValidationOK) {
    return std::string();
  }

  enterprise_management::DeviceManagementRequest dm_request;

  enterprise_management::PolicyValidationReportRequest*
      policy_validation_report_request =
          dm_request.mutable_policy_validation_report_request();
  policy_validation_report_request->set_validation_result_type(
      TranslatePolicyValidationResultStatus(aggregated_status));
  policy_validation_report_request->set_policy_type(
      validation_result.policy_type);
  policy_validation_report_request->set_policy_token(
      validation_result.policy_token);

  for (const PolicyValueValidationIssue& issue : validation_result.issues) {
    enterprise_management::PolicyValueValidationIssue*
        policy_value_validation_issue =
            policy_validation_report_request
                ->add_policy_value_validation_issues();
    policy_value_validation_issue->set_policy_name(issue.policy_name);
    policy_value_validation_issue->set_severity(
        TranslatePolicyValidationResultSeverity(issue.severity));
    policy_value_validation_issue->set_debug_message(issue.message);
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

bool ShouldDeleteDmToken(const std::string& response_data) {
  enterprise_management::DeviceManagementResponse dm_response;
  return dm_response.ParseFromString(response_data) &&
         base::ranges::find(dm_response.error_detail(),
                            enterprise_management::
                                CBCM_DELETION_POLICY_PREFERENCE_DELETE_TOKEN) !=
             dm_response.error_detail().end();
}

DMPolicyMap ParsePolicyFetchResponse(
    const std::string& response_data,
    const device_management_storage::CachedPolicyInfo& policy_info,
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

    if (!validator.ValidatePolicyResponse(response, validation_result)) {
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
