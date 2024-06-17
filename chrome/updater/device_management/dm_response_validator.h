// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_DEVICE_MANAGEMENT_DM_RESPONSE_VALIDATOR_H_
#define CHROME_UPDATER_DEVICE_MANAGEMENT_DM_RESPONSE_VALIDATOR_H_

#include <string>
#include <vector>

#include "base/ranges/algorithm.h"
#include "chrome/enterprise_companion/device_management_storage/dm_storage.h"

namespace enterprise_management {

class PolicyData;
class PolicyFetchResponse;

}  // namespace enterprise_management

namespace updater {

struct PolicyValueValidationIssue {
  enum class Severity { kWarning, kError };

  PolicyValueValidationIssue(const std::string& policy_name,
                             Severity severity,
                             const std::string& message);
  ~PolicyValueValidationIssue();

  bool operator==(const PolicyValueValidationIssue& other) const {
    return policy_name == other.policy_name && severity == other.severity &&
           message == other.message;
  }

  std::string policy_name;
  Severity severity = Severity::kWarning;
  std::string message;
};

struct PolicyValidationResult {
  enum class Status {
    // Indicates successful validation.
    kValidationOK,
    // Bad signature on the initial key.
    kValidationBadInitialSignature,
    // Bad signature.
    kValidationBadSignature,
    // Policy blob contains error code.
    kValidationErrorCodePresent,
    // Policy payload failed to decode.
    kValidationPayloadParseError,
    // Unexpected policy type.
    kValidationWrongPolicyType,
    // Unexpected settings entity id.
    kValidationWrongSettingsEntityID,
    // Timestamp is missing or is older than expected.
    kValidationBadTimestamp,
    // DM token is empty or doesn't match.
    kValidationBadDMToken,
    // Device id is empty or doesn't match.
    kValidationBadDeviceID,
    // User id doesn't match.
    kValidationBadUser,
    // Policy payload protobuf parse error.
    kValidationPolicyParseError,
    // Policy key signature could not be verified using the hard-coded
    // verification key.
    kValidationBadKeyVerificationSignature,
    // Policy value validation raised warning(s).
    kValidationValueWarning,
    // Policy value validation failed with error(s).
    kValidationValueError,
  };

  PolicyValidationResult();
  PolicyValidationResult(const PolicyValidationResult& other);
  ~PolicyValidationResult();

  bool HasErrorIssue() const {
    return base::ranges::any_of(issues, [](const auto& issue) {
      return issue.severity == PolicyValueValidationIssue::Severity::kError;
    });
  }

  bool operator==(const PolicyValidationResult& other) const {
    return policy_type == other.policy_type &&
           policy_token == other.policy_token && status == other.status &&
           issues == other.issues;
  }

  std::string policy_type;
  std::string policy_token;

  Status status = Status::kValidationOK;
  std::vector<PolicyValueValidationIssue> issues;
};

class DMResponseValidator {
 public:
  DMResponseValidator(
      const device_management_storage::CachedPolicyInfo& policy_info,
      const std::string& expected_dm_token,
      const std::string& expected_device_id);
  ~DMResponseValidator();

  // Validates a single policy fetch response.
  // The validation steps are sorted in the order of descending severity
  // of the error, that means the most severe check will determine the
  // validation status.
  bool ValidatePolicyResponse(
      const enterprise_management::PolicyFetchResponse& policy_response,
      PolicyValidationResult& validation_result) const;

  // Validates that the policy response data is properly signed.
  bool ValidatePolicyData(
      const enterprise_management::PolicyFetchResponse& policy_response) const;

 private:
  // Extracts and validates the public key for for subsequent policy
  // verification. The public key is either the new key that signed the
  // response, or the existing public key if key is not rotated.
  // Possible scenarios:
  // 1) Client sends the first policy fetch request. In this case, there's no
  //    existing public key on the client side. The server must attach a new
  //    public key in the response and the new key must be signed by the pinned
  //    key.
  // 2) Client sends a policy fetch request with an existing public key, and
  //    server decides to NOT rotate the key. In this case, the response doesn't
  //    have a new key. The signing key will continue to be the existing key.
  // 3) Client sends a policy fetch request with an existing public key, and
  //    server decides to rotate the key. In this case, the server attaches a
  //    new public key in the response. The new key must be signed by the pinned
  //    key AND the existing key.
  //
  // `signature_key` will be the new public key when it is available, otherwise
  // it will be the existing public key from cached policy info.
  bool ValidateNewPublicKey(
      const enterprise_management::PolicyFetchResponse& fetch_response,
      std::string& signature_key,
      PolicyValidationResult& validation_result) const;

  bool ValidateSignature(
      const enterprise_management::PolicyFetchResponse& fetch_response,
      const std::string& signature_key,
      PolicyValidationResult& validation_result) const;
  bool ValidateDMToken(const enterprise_management::PolicyData& policy_data,
                       PolicyValidationResult& validation_result) const;
  bool ValidateDeviceId(const enterprise_management::PolicyData& policy_data,
                        PolicyValidationResult& validation_result) const;
  bool ValidateTimestamp(const enterprise_management::PolicyData& policy_data,
                         PolicyValidationResult& validation_result) const;
  bool ValidatePayloadPolicy(
      const enterprise_management::PolicyData& policy_data,
      PolicyValidationResult& validation_result) const;

  const device_management_storage::CachedPolicyInfo policy_info_;
  const std::string expected_dm_token_;
  const std::string expected_device_id_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_DEVICE_MANAGEMENT_DM_RESPONSE_VALIDATOR_H_
