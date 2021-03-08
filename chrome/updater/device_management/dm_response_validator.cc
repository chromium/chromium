// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/device_management/dm_response_validator.h"

#include <string>

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/updater/device_management/dm_cached_policy_info.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/signature_verifier.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

namespace updater {

namespace {

bool VerifySHA256Signature(const std::string& data,
                           const std::string& key,
                           const std::string& signature) {
  crypto::SignatureVerifier verifier;
  if (!verifier.VerifyInit(crypto::SignatureVerifier::RSA_PKCS1_SHA256,
                           base::as_bytes(base::make_span(signature)),
                           base::as_bytes(base::make_span(key)))) {
    VLOG(1) << "Invalid verification signature/key format.";
    return false;
  }
  verifier.VerifyUpdate(base::as_bytes(base::make_span(data)));
  return verifier.VerifyFinal();
}

}  // namespace

PolicyValueValidationIssue::PolicyValueValidationIssue(
    const std::string& policy_name,
    Severity severity,
    const std::string& message)
    : policy_name(policy_name), severity(severity), message(message) {}
PolicyValueValidationIssue::~PolicyValueValidationIssue() = default;

PolicyValidationResult::PolicyValidationResult() = default;
PolicyValidationResult::PolicyValidationResult(
    const PolicyValidationResult& other) = default;
PolicyValidationResult::~PolicyValidationResult() = default;

DMResponseValidator::DMResponseValidator(const CachedPolicyInfo& policy_info,
                                         const std::string& expected_dm_token,
                                         const std::string& expected_device_id)
    : policy_info_(policy_info),
      expected_dm_token_(expected_dm_token),
      expected_device_id_(expected_device_id) {}

DMResponseValidator::~DMResponseValidator() = default;

bool DMResponseValidator::ValidateNewPublicKey(
    const enterprise_management::PolicyFetchResponse& fetch_response,
    std::string& signature_key,
    PolicyValidationResult& validation_result) const {
  if (!fetch_response.has_new_public_key_verification_data()) {
    // No new public key, meaning key is not rotated, so use the existing one.
    if (policy_info_.public_key().empty()) {
      VLOG(1) << "No existing or new public key, must have one at least.";
      validation_result.status =
          PolicyValidationResult::Status::kValidationBadSignature;
      return false;
    }

    signature_key = policy_info_.public_key();
    return true;
  }

  if (!fetch_response.has_new_public_key_verification_data_signature()) {
    VLOG(1) << "New public key doesn't have signature for verification.";
    validation_result.status =
        PolicyValidationResult::Status::kValidationBadKeyVerificationSignature;
    return false;
  }

  // Verifies that the new public key verification data is properly signed
  // by the pinned key.
  if (!VerifySHA256Signature(
          fetch_response.new_public_key_verification_data(),
          policy::GetPolicyVerificationKey(),
          fetch_response.new_public_key_verification_data_signature())) {
    VLOG(1) << "Public key verification data is not signed correctly.";
    validation_result.status =
        PolicyValidationResult::Status::kValidationBadKeyVerificationSignature;
    return false;
  }

  // Also validates new public key against the cached public key, if the latter
  // exists (The server must sign the new key with the previous key).
  enterprise_management::PublicKeyVerificationData public_key_data;
  if (!public_key_data.ParseFromString(
          fetch_response.new_public_key_verification_data())) {
    VLOG(1) << "Failed to deserialize new public key.";
    validation_result.status =
        PolicyValidationResult::Status::kValidationPayloadParseError;
    return false;
  }

  const std::string existing_key = policy_info_.public_key();
  if (!existing_key.empty()) {
    if (!fetch_response.has_new_public_key_signature() ||
        !VerifySHA256Signature(public_key_data.new_public_key(), existing_key,
                               fetch_response.new_public_key_signature())) {
      VLOG(1) << "Key verification against cached public key failed.";
      validation_result.status = PolicyValidationResult::Status::
          kValidationBadKeyVerificationSignature;
      return false;
    }
  }

  // Now that the new public key has been successfully verified, we will use it
  // for future policy data validation.
  VLOG(1) << "Accepting a public key for domain: " << public_key_data.domain();
  signature_key = public_key_data.new_public_key();
  return true;
}

bool DMResponseValidator::ValidateSignature(
    const enterprise_management::PolicyFetchResponse& policy_response,
    const std::string& signature_key,
    PolicyValidationResult& validation_result) const {
  if (!policy_response.has_policy_data_signature()) {
    VLOG(1) << "Policy entry does not have verification signature.";
    validation_result.status =
        PolicyValidationResult::Status::kValidationBadSignature;
    return false;
  }

  const std::string& policy_data = policy_response.policy_data();
  if (!VerifySHA256Signature(policy_data, signature_key,
                             policy_response.policy_data_signature())) {
    validation_result.status =
        PolicyValidationResult::Status::kValidationBadSignature;
    return false;
  }

  // Our signing key signs both the policy data and the previous key. In
  // theory it is possible to have a cross-protocol attack here: attacker can
  // take a signed public key and claim it is the policy data. Check that
  // the policy data is not a public key to defend against such attacks.
  bssl::UniquePtr<RSA> public_key(RSA_public_key_from_bytes(
      reinterpret_cast<const uint8_t*>(policy_data.data()),
      policy_data.length()));
  if (public_key) {
    VLOG(1) << "Rejected policy data in public key format.";
    validation_result.status =
        PolicyValidationResult::Status::kValidationBadSignature;
    return false;
  }

  return true;
}

bool DMResponseValidator::ValidateDMToken(
    const enterprise_management::PolicyData& policy_data,
    PolicyValidationResult& validation_result) const {
  if (!policy_data.has_request_token()) {
    VLOG(1) << "No DMToken in PolicyData.";
    validation_result.status =
        PolicyValidationResult::Status::kValidationBadDMToken;
    return false;
  }

  const std::string& received_token = policy_data.request_token();
  if (!base::EqualsCaseInsensitiveASCII(received_token, expected_dm_token_)) {
    VLOG(1) << "Unexpected DMToken: expected " << expected_dm_token_
            << ", received " << received_token;
    validation_result.status =
        PolicyValidationResult::Status::kValidationBadDMToken;
    return false;
  }

  return true;
}

bool DMResponseValidator::ValidateDeviceId(
    const enterprise_management::PolicyData& policy_data,
    PolicyValidationResult& validation_result) const {
  if (!policy_data.has_device_id()) {
    VLOG(1) << "No Device Id in PolicyData.";
    validation_result.status =
        PolicyValidationResult::Status::kValidationBadDeviceID;
    return false;
  }

  const std::string& received_id = policy_data.device_id();
  if (!base::EqualsCaseInsensitiveASCII(received_id, expected_device_id_)) {
    VLOG(1) << "Unexpected Device Id: expected " << expected_device_id_
            << ", received " << received_id;
    validation_result.status =
        PolicyValidationResult::Status::kValidationBadDeviceID;
    return false;
  }

  return true;
}

bool DMResponseValidator::ValidateTimestamp(
    const enterprise_management::PolicyData& policy_data,
    PolicyValidationResult& validation_result) const {
  if (!policy_data.has_timestamp()) {
    VLOG(1) << "No timestamp in PolicyData.";
    validation_result.status =
        PolicyValidationResult::Status::kValidationBadTimestamp;
    return false;
  }

  if (policy_data.timestamp() < policy_info_.timestamp()) {
    VLOG(1) << "Unexpected DM response timestamp older than cached timestamp.";
    validation_result.status =
        PolicyValidationResult::Status::kValidationBadTimestamp;
    return false;
  }

  return true;
}

bool DMResponseValidator::ValidatePolicy(
    const enterprise_management::PolicyFetchResponse& fetch_response,
    PolicyValidationResult& validation_result) const {
  enterprise_management::PolicyData fetch_policy_data;
  if (!fetch_response.has_policy_data() ||
      !fetch_policy_data.ParseFromString(fetch_response.policy_data()) ||
      !fetch_policy_data.IsInitialized()) {
    VLOG(1) << "Missing or invalid PolicyData in policy response.";
    validation_result.status =
        PolicyValidationResult::Status::kValidationPolicyParseError;
    return false;
  }

  if (fetch_policy_data.has_policy_token())
    validation_result.policy_token = fetch_policy_data.policy_token();

  if (!ValidateDMToken(fetch_policy_data, validation_result) ||
      !ValidateDeviceId(fetch_policy_data, validation_result) ||
      !ValidateTimestamp(fetch_policy_data, validation_result)) {
    return false;
  }

  std::string signature_key;
  if (!ValidateNewPublicKey(fetch_response, signature_key, validation_result))
    return false;

  if (fetch_policy_data.has_policy_type())
    validation_result.policy_type = fetch_policy_data.policy_type();
  if (validation_result.policy_type.empty()) {
    VLOG(1) << "Missing policy type in the policy response.";
    validation_result.status =
        PolicyValidationResult::Status::kValidationWrongPolicyType;
    return false;
  }

  if (!ValidateSignature(fetch_response, signature_key, validation_result)) {
    VLOG(1) << "Failed to verify the signature for policy "
            << validation_result.policy_type;
    return false;
  }

  // TODO(crbug/1183453): Further validate Omaha policies if the policy type is
  // "google/machine-level-omaha".

  return true;
}

}  // namespace updater
