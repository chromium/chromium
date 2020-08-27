// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/device_management/dm_message.h"

#include <memory>
#include <utility>

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

class DMResponseValidator {
 public:
  DMResponseValidator(const CachedPolicyInfo& policy_info,
                      const std::string& expected_dm_token,
                      const std::string& expected_device_id);
  ~DMResponseValidator() = default;

  static std::unique_ptr<DMResponseValidator> Create(
      const CachedPolicyInfo& policy_info,
      const std::string& expected_dm_token,
      const std::string& expected_device_id,
      const enterprise_management::DeviceManagementResponse& dm_response);

  // Validates the DM response is correctly signed and intended for this device.
  bool ValidateResponse(
      const enterprise_management::DeviceManagementResponse& dm_response) const;

  // Validates a single policy inside the DM response is correctly signed.
  bool ValidatePolicy(
      const enterprise_management::PolicyFetchResponse& policy_response) const;

 private:
  // Extracts the public key for for subsequent policy verification. The public
  // key is either the new key that signed the response, or the existing public
  // key if key is not rotated. Possible scenarios:
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
  bool ExtractPublicKey(
      const enterprise_management::DeviceManagementResponse& dm_response);

  bool ValidateDMToken(
      const enterprise_management::PolicyData& policy_data) const;
  bool ValidateDeviceId(
      const enterprise_management::PolicyData& policy_data) const;
  bool ValidateTimestamp(
      const enterprise_management::PolicyData& policy_data) const;

  const CachedPolicyInfo policy_info_;
  const std::string expected_dm_token_;
  const std::string expected_device_id_;

  std::string signing_public_key_;
};

DMResponseValidator::DMResponseValidator(const CachedPolicyInfo& policy_info,
                                         const std::string& expected_dm_token,
                                         const std::string& expected_device_id)
    : policy_info_(policy_info),
      expected_dm_token_(expected_dm_token),
      expected_device_id_(expected_device_id) {}

// static
std::unique_ptr<DMResponseValidator> DMResponseValidator::Create(
    const CachedPolicyInfo& policy_info,
    const std::string& expected_dm_token,
    const std::string& expected_device_id,
    const enterprise_management::DeviceManagementResponse& dm_response) {
  auto validator = std::make_unique<DMResponseValidator>(
      policy_info, expected_dm_token, expected_device_id);
  if (!validator->ExtractPublicKey(dm_response))
    return nullptr;

  return validator;
}

bool DMResponseValidator::ExtractPublicKey(
    const enterprise_management::DeviceManagementResponse& dm_response) {
  // We should only extract public key once.
  DCHECK(signing_public_key_.empty());

  if (!dm_response.has_policy_response() ||
      dm_response.policy_response().responses_size() == 0) {
    return false;
  }

  // We can extract the public key from any of the policy in the response. For
  // convenience, just use the first policy.
  const enterprise_management::PolicyFetchResponse& first_policy_response =
      dm_response.policy_response().responses(0);

  if (!first_policy_response.has_new_public_key_verification_data()) {
    // No new public key, meaning key is not rotated, so use the existing one.
    signing_public_key_ = policy_info_.public_key();
    if (signing_public_key_.empty()) {
      VLOG(1) << "No existing or new public key, must have one at least.";
      return false;
    }

    return true;
  }

  if (!first_policy_response.has_new_public_key_verification_data_signature()) {
    VLOG(1) << "New public key doesn't have signature for verification.";
    return false;
  }

  // Verifies that the new public key verification data is properly signed
  // by the pinned key.
  if (!VerifySHA256Signature(
          first_policy_response.new_public_key_verification_data(),
          policy::GetPolicyVerificationKey(),
          first_policy_response.new_public_key_verification_data_signature())) {
    VLOG(1) << "Public key verification data is not signed correctly.";
    return false;
  }

  // Also validates new public key against the cached public key, if the latter
  // exists (The server must sign the new key with the previous key).
  enterprise_management::PublicKeyVerificationData public_key_data;
  if (!public_key_data.ParseFromString(
          first_policy_response.new_public_key_verification_data())) {
    VLOG(1) << "Failed to deserialize new public key.";
    return false;
  }

  const std::string existing_key = policy_info_.public_key();
  if (!existing_key.empty()) {
    if (!first_policy_response.has_new_public_key_signature() ||
        !VerifySHA256Signature(
            public_key_data.new_public_key(), existing_key,
            first_policy_response.new_public_key_signature())) {
      VLOG(1) << "Key verification against cached public key failed.";
      return false;
    }
  }

  // Now that the new public key has been successfully verified, we rotate to
  // use it for future policy data validation.
  VLOG(1) << "Accepting a public key for domain: " << public_key_data.domain();
  signing_public_key_ = public_key_data.new_public_key();
  return true;
}

bool DMResponseValidator::ValidateDMToken(
    const enterprise_management::PolicyData& policy_data) const {
  if (!policy_data.has_request_token()) {
    VLOG(1) << "No DMToken in PolicyData.";
    return false;
  }

  const std::string& received_token = policy_data.request_token();
  if (!base::EqualsCaseInsensitiveASCII(received_token, expected_dm_token_)) {
    VLOG(1) << "Unexpected DMToken: expected " << expected_dm_token_
            << ", received " << received_token;
    return false;
  }

  return true;
}

bool DMResponseValidator::ValidateDeviceId(
    const enterprise_management::PolicyData& policy_data) const {
  if (!policy_data.has_device_id()) {
    VLOG(1) << "No Device Id in PolicyData.";
    return false;
  }

  const std::string& received_id = policy_data.device_id();
  if (!base::EqualsCaseInsensitiveASCII(received_id, expected_device_id_)) {
    VLOG(1) << "Unexpected Device Id: expected " << expected_device_id_
            << ", received " << received_id;
    return false;
  }

  return true;
}

bool DMResponseValidator::ValidateTimestamp(
    const enterprise_management::PolicyData& policy_data) const {
  if (!policy_data.has_timestamp()) {
    VLOG(1) << "No timestamp in PolicyData.";
    return false;
  }

  if (policy_data.timestamp() < policy_info_.timestamp()) {
    VLOG(1) << "Unexpected DM response timestamp older than cached timestamp.";
    return false;
  }

  return true;
}

bool DMResponseValidator::ValidateResponse(
    const enterprise_management::DeviceManagementResponse& dm_response) const {
  if (!dm_response.has_policy_response() ||
      dm_response.policy_response().responses_size() == 0) {
    return false;
  }

  // We can validate the DM response with any of the policy in it. For
  // convenience, just use the first policy.
  const enterprise_management::PolicyFetchResponse& first_policy_response =
      dm_response.policy_response().responses(0);

  enterprise_management::PolicyData policy_data;
  if (!policy_data.ParseFromString(first_policy_response.policy_data())) {
    VLOG(1) << "Failed to deserialize policy data.";
    return false;
  }

  return ValidateDMToken(policy_data) && ValidateDeviceId(policy_data) &&
         ValidateTimestamp(policy_data);
}

bool DMResponseValidator::ValidatePolicy(
    const enterprise_management::PolicyFetchResponse& policy_response) const {
  if (!policy_response.has_policy_data()) {
    VLOG(1) << "Policy entry does not have data.";
    return false;
  }

  if (!policy_response.has_policy_data_signature()) {
    VLOG(1) << "Policy entry does not have verification signature.";
    return false;
  }

  const std::string& policy_data = policy_response.policy_data();
  if (!VerifySHA256Signature(policy_data, signing_public_key_,
                             policy_response.policy_data_signature())) {
    VLOG(1) << "Policy entry are not correctly signed.";
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
    return false;
  }

  return true;
}

}  // namespace

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

DMPolicyMap ParsePolicyFetchResponse(const std::string& response_data,
                                     const CachedPolicyInfo& policy_info,
                                     const std::string& expected_dm_token,
                                     const std::string& expected_device_id) {
  enterprise_management::DeviceManagementResponse dm_response;
  if (!dm_response.ParseFromString(response_data) ||
      !dm_response.has_policy_response() ||
      dm_response.policy_response().responses_size() == 0) {
    return {};
  }

  std::unique_ptr<DMResponseValidator> validator = DMResponseValidator::Create(
      policy_info, expected_dm_token, expected_device_id, dm_response);
  if (!validator || !validator->ValidateResponse(dm_response)) {
    return {};
  }

  // Validate each individual policy and put valid ones into the returned policy
  // map.
  DMPolicyMap responses;
  for (int i = 0; i < dm_response.policy_response().responses_size(); ++i) {
    const ::enterprise_management::PolicyFetchResponse& response =
        dm_response.policy_response().responses(i);
    enterprise_management::PolicyData policy_data;
    if (!response.has_policy_data() ||
        !policy_data.ParseFromString(response.policy_data()) ||
        !policy_data.IsInitialized() || !policy_data.has_policy_type()) {
      VLOG(1) << "Ignoring invalid PolicyData.";
      continue;
    }

    const std::string& policy_type = policy_data.policy_type();
    if (!validator->ValidatePolicy(response)) {
      VLOG(1) << "Policy " << policy_type << " validation failed.";
      continue;
    }

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
