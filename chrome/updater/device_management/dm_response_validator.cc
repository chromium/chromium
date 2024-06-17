// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/device_management/dm_response_validator.h"

#include <inttypes.h>

#include <string>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/enterprise_companion/device_management_storage/dm_storage.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/device_management/dm_message.h"
#include "chrome/updater/protos/omaha_settings.pb.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/signature_verifier.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

namespace updater {

namespace {

namespace edm = ::wireless_android_enterprise_devicemanagement;

constexpr const char* kProxyModeValidValues[] = {
    kProxyModeDirect,       kProxyModeAutoDetect, kProxyModePacScript,
    kProxyModeFixedServers, kProxyModeSystem,
};

crypto::SignatureVerifier::SignatureAlgorithm GetResponseSignatureType(
    const enterprise_management::PolicyFetchResponse& fetch_response) {
  if (!fetch_response.has_policy_data_signature_type()) {
    VLOG(1) << "No signature type in response, assume SHA256.";
    return crypto::SignatureVerifier::RSA_PKCS1_SHA256;
  }

  switch (fetch_response.policy_data_signature_type()) {
    case enterprise_management::PolicyFetchRequest::SHA1_RSA:
      VLOG(1) << "Response is signed with deprecated SHA1 algorithm.";
      return crypto::SignatureVerifier::RSA_PKCS1_SHA1;
    case enterprise_management::PolicyFetchRequest::SHA256_RSA:
      return crypto::SignatureVerifier::RSA_PKCS1_SHA256;
    default:
      VLOG(1) << "Unrecognized signature type in response, assume SHA256.";
      return crypto::SignatureVerifier::RSA_PKCS1_SHA256;
  }
}

bool VerifySignature(const std::string& data,
                     const std::string& key,
                     const std::string& signature,
                     crypto::SignatureVerifier::SignatureAlgorithm algorithm) {
  crypto::SignatureVerifier verifier;
  if (!verifier.VerifyInit(algorithm,
                           base::as_bytes(base::make_span(signature)),
                           base::as_bytes(base::make_span(key)))) {
    VLOG(1) << "Invalid verification signature/key format.";
    return false;
  }
  verifier.VerifyUpdate(base::as_bytes(base::make_span(data)));
  return verifier.VerifyFinal();
}

class OmahaPolicyValidator {
 public:
  OmahaPolicyValidator() = default;
  ~OmahaPolicyValidator() = default;

  bool Initialize(const enterprise_management::PolicyData& policy_data);
  bool Validate(PolicyValidationResult& validation_result) const;

 private:
  // Functions to validate global-level policies.
  void ValidateAutoUpdateCheckPeriodPolicy(
      PolicyValidationResult& result) const;
  void ValidateDownloadPreferencePolicy(PolicyValidationResult& result) const;
  void ValidateUpdatesSuppressedPolicies(PolicyValidationResult& result) const;
  void ValidateProxyPolicies(PolicyValidationResult& result) const;

  // Functions to validate app-level policies.
  void ValidateAppTargetChannelPolicy(
      const edm::ApplicationSettings& app_settings,
      PolicyValidationResult& validation_result) const;
  void ValidateAppTargetVersionPrefixPolicy(
      const edm::ApplicationSettings& app_settings,
      PolicyValidationResult& validation_result) const;

  edm::OmahaSettingsClientProto omaha_settings_;
};

bool OmahaPolicyValidator::Initialize(
    const enterprise_management::PolicyData& policy_data) {
  return omaha_settings_.ParseFromString(policy_data.policy_value());
}

bool OmahaPolicyValidator::Validate(
    PolicyValidationResult& validation_result) const {
  ValidateAutoUpdateCheckPeriodPolicy(validation_result);
  ValidateDownloadPreferencePolicy(validation_result);
  ValidateUpdatesSuppressedPolicies(validation_result);
  ValidateProxyPolicies(validation_result);

  for (const auto& app_settings : omaha_settings_.application_settings()) {
    ValidateAppTargetChannelPolicy(app_settings, validation_result);
    ValidateAppTargetVersionPrefixPolicy(app_settings, validation_result);
  }

  return validation_result.status ==
             PolicyValidationResult::Status::kValidationOK &&
         !validation_result.HasErrorIssue();
}

void OmahaPolicyValidator::ValidateAutoUpdateCheckPeriodPolicy(
    PolicyValidationResult& validation_result) const {
  if (omaha_settings_.has_auto_update_check_period_minutes() &&
      (omaha_settings_.auto_update_check_period_minutes() < 0 ||
       omaha_settings_.auto_update_check_period_minutes() >
           kMaxAutoUpdateCheckPeriodMinutes)) {
    validation_result.issues.emplace_back(
        "auto_update_check_period_minutes",
        PolicyValueValidationIssue::Severity::kError,
        base::StringPrintf("Value out of range (0 - %d): %" PRId64,
                           kMaxAutoUpdateCheckPeriodMinutes,
                           omaha_settings_.auto_update_check_period_minutes()));
  }
}

void OmahaPolicyValidator::ValidateDownloadPreferencePolicy(
    PolicyValidationResult& validation_result) const {
  if (!omaha_settings_.has_download_preference()) {
    return;
  }

  if (!base::EqualsCaseInsensitiveASCII(omaha_settings_.download_preference(),
                                        kDownloadPreferenceCacheable)) {
    validation_result.issues.emplace_back(
        "download_preference", PolicyValueValidationIssue::Severity::kWarning,
        "Unrecognized download preference: " +
            omaha_settings_.download_preference());
  }
}
void OmahaPolicyValidator::ValidateUpdatesSuppressedPolicies(
    PolicyValidationResult& validation_result) const {
  if (!omaha_settings_.has_updates_suppressed()) {
    return;
  }

  if (omaha_settings_.updates_suppressed().start_hour() < 0 ||
      omaha_settings_.updates_suppressed().start_hour() >= 24) {
    validation_result.issues.emplace_back(
        "updates_suppressed.start_hour",
        PolicyValueValidationIssue::Severity::kError,
        base::StringPrintf("Value out of range(0 - 23): %" PRId64,
                           omaha_settings_.updates_suppressed().start_hour()));
  }
  if (omaha_settings_.updates_suppressed().start_minute() < 0 ||
      omaha_settings_.updates_suppressed().start_minute() >= 60) {
    validation_result.issues.emplace_back(
        "updates_suppressed.start_minute",
        PolicyValueValidationIssue::Severity::kError,
        base::StringPrintf(
            "Value out of range(0 - 59): %" PRId64,
            omaha_settings_.updates_suppressed().start_minute()));
  }
  if (omaha_settings_.updates_suppressed().duration_min() < 0 ||
      omaha_settings_.updates_suppressed().duration_min() >
          kMaxUpdatesSuppressedDurationMinutes) {
    validation_result.issues.emplace_back(
        "updates_suppressed.duration_min",
        PolicyValueValidationIssue::Severity::kError,
        base::StringPrintf(
            "Value out of range(0 - %d): %" PRId64,
            kMaxUpdatesSuppressedDurationMinutes,
            omaha_settings_.updates_suppressed().duration_min()));
  }
}

void OmahaPolicyValidator::ValidateProxyPolicies(
    PolicyValidationResult& validation_result) const {
  if (omaha_settings_.has_proxy_mode()) {
    const std::string proxy_mode =
        base::ToLowerASCII(omaha_settings_.proxy_mode());
    if (!base::Contains(kProxyModeValidValues, proxy_mode)) {
      validation_result.issues.emplace_back(
          "proxy_mode", PolicyValueValidationIssue::Severity::kWarning,
          "Unrecognized proxy mode: " + omaha_settings_.proxy_mode());
    }
  }

  if (omaha_settings_.has_proxy_server()) {
    if (!omaha_settings_.has_proxy_mode()) {
      validation_result.issues.emplace_back(
          "proxy_server", PolicyValueValidationIssue::Severity::kWarning,
          "Proxy server setting is ignored because proxy mode is not set.");
    } else {
      if (!base::EqualsCaseInsensitiveASCII(omaha_settings_.proxy_mode(),
                                            kProxyModeFixedServers)) {
        validation_result.issues.emplace_back(
            "proxy_server", PolicyValueValidationIssue::Severity::kWarning,
            base::StringPrintf("Proxy server setting [%s] is ignored "
                               "because proxy mode is not [%s]",
                               omaha_settings_.proxy_server().c_str(),
                               kProxyModeFixedServers));
      }
    }
  }

  if (omaha_settings_.has_proxy_pac_url()) {
    if (!omaha_settings_.has_proxy_mode()) {
      validation_result.issues.emplace_back(
          "proxy_pac_url", PolicyValueValidationIssue::Severity::kWarning,
          "Proxy PAC URL setting is ignored because proxy mode is not set.");
    } else {
      if (!base::EqualsCaseInsensitiveASCII(omaha_settings_.proxy_mode(),
                                            kProxyModePacScript)) {
        validation_result.issues.emplace_back(
            "proxy_pac_url", PolicyValueValidationIssue::Severity::kWarning,
            base::StringPrintf("Proxy PAC URL setting [%s] is ignored because "
                               "proxy mode is not [%s]",
                               omaha_settings_.proxy_pac_url().c_str(),
                               kProxyModePacScript));
      }
    }
  }
}

void OmahaPolicyValidator::ValidateAppTargetChannelPolicy(
    const edm::ApplicationSettings& app_settings,
    PolicyValidationResult& validation_result) const {
  if (app_settings.has_target_channel() &&
      app_settings.target_channel().empty()) {
    validation_result.issues.emplace_back(
        "target_channel", PolicyValueValidationIssue::Severity::kWarning,
        app_settings.app_guid() + " empty policy value");
  }
}

void OmahaPolicyValidator::ValidateAppTargetVersionPrefixPolicy(
    const edm::ApplicationSettings& app_settings,
    PolicyValidationResult& validation_result) const {
  if (app_settings.has_target_version_prefix() &&
      app_settings.target_version_prefix().empty()) {
    validation_result.issues.emplace_back(
        "target_version_prefix", PolicyValueValidationIssue::Severity::kWarning,
        app_settings.app_guid() + " empty policy value");
  }
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

DMResponseValidator::DMResponseValidator(
    const device_management_storage::CachedPolicyInfo& policy_info,
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
  // by the pinned key. The DM server always signs the new key using SHA256
  // algorithm.
  if (!VerifySignature(
          fetch_response.new_public_key_verification_data(),
          policy::GetPolicyVerificationKey(),
          fetch_response.new_public_key_verification_data_signature(),
          crypto::SignatureVerifier::RSA_PKCS1_SHA256)) {
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
        !VerifySignature(public_key_data.new_public_key(), existing_key,
                         fetch_response.new_public_key_signature(),
                         GetResponseSignatureType(fetch_response))) {
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
  if (!VerifySignature(policy_data, signature_key,
                       policy_response.policy_data_signature(),
                       GetResponseSignatureType(policy_response))) {
    VLOG(1) << "Policy signature validation failed.";
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
    VLOG(1) << "Unexpected DM response timestamp [" << policy_data.timestamp()
            << "] is older than cached timestamp [" << policy_info_.timestamp()
            << "].";
    validation_result.status =
        PolicyValidationResult::Status::kValidationBadTimestamp;
    return false;
  }

  return true;
}

bool DMResponseValidator::ValidatePayloadPolicy(
    const enterprise_management::PolicyData& policy_data,
    PolicyValidationResult& validation_result) const {
  // Policy type was validated previously.
  CHECK(policy_data.has_policy_type());

  if (base::EqualsCaseInsensitiveASCII(policy_data.policy_type(),
                                       kGoogleUpdatePolicyType)) {
    OmahaPolicyValidator validator;
    if (!validator.Initialize(policy_data)) {
      validation_result.status =
          PolicyValidationResult::Status::kValidationPolicyParseError;
      return false;
    }
    return validator.Validate(validation_result);
  }

  return true;
}

bool DMResponseValidator::ValidatePolicyResponse(
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

  if (!fetch_policy_data.has_policy_type()) {
    VLOG(1) << "Missing policy type in the policy response.";
    validation_result.status =
        PolicyValidationResult::Status::kValidationWrongPolicyType;
    return false;
  }
  validation_result.policy_type = fetch_policy_data.policy_type();

  if (fetch_policy_data.has_policy_token()) {
    validation_result.policy_token = fetch_policy_data.policy_token();
  }

  if (!ValidateDMToken(fetch_policy_data, validation_result) ||
      !ValidateDeviceId(fetch_policy_data, validation_result) ||
      !ValidateTimestamp(fetch_policy_data, validation_result)) {
    return false;
  }

  std::string signature_key;
  if (!ValidateNewPublicKey(fetch_response, signature_key, validation_result)) {
    return false;
  }

  if (!ValidateSignature(fetch_response, signature_key, validation_result)) {
    VLOG(1) << "Failed to verify the signature for policy "
            << validation_result.policy_type;
    return false;
  }

  if (!ValidatePayloadPolicy(fetch_policy_data, validation_result)) {
    VLOG(1) << "Payload policy validation failed, policy type: "
            << validation_result.policy_type;
    return false;
  }

  return true;
}

bool DMResponseValidator::ValidatePolicyData(
    const enterprise_management::PolicyFetchResponse& fetch_response) const {
  PolicyValidationResult validation_result;
  return ValidateSignature(fetch_response, policy_info_.public_key(),
                           validation_result);
}

}  // namespace updater
