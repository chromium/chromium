// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_validator.h"

#include <stddef.h>
#include <utility>

#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/signature_verifier.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace em = enterprise_management;

namespace policy {

namespace {

const char kMetricPolicyKeyVerification[] = "Enterprise.PolicyKeyVerification";

enum MetricPolicyKeyVerification {
  // Obsolete. Kept to avoid reuse, as this is used in histograms.
  // UMA metric recorded when the client has no verification key.
  METRIC_POLICY_KEY_VERIFICATION_KEY_MISSING_DEPRECATED,
  // Recorded when the policy being verified has no key signature (e.g. policy
  // fetched before the server supported the verification key).
  METRIC_POLICY_KEY_VERIFICATION_SIGNATURE_MISSING,
  // Recorded when the key signature did not match the expected value (in
  // theory, this should only happen after key rotation or if the policy cached
  // on disk has been modified).
  METRIC_POLICY_KEY_VERIFICATION_FAILED,
  // Recorded when key verification succeeded.
  METRIC_POLICY_KEY_VERIFICATION_SUCCEEDED,
  METRIC_POLICY_KEY_VERIFICATION_SIZE  // Must be the last.
};

const char kMetricPolicyUserVerification[] =
    "Enterprise.PolicyUserVerification";

enum class MetricPolicyUserVerification {
  // Gaia id check used, but failed.
  kGaiaIdFailed = 0,
  // Gaia id check used and succeeded.
  kGaiaIdSucceeded = 1,
  // Gaia id is not present and username check failed.
  kUsernameFailed = 2,
  // Gaia id is not present for user and username check succeeded.
  kUsernameSucceeded = 3,
  // Gaia id is not present in policy and username check succeeded.
  kGaiaIdMissingUsernameSucceeded = 4,
  kMaxValue = kGaiaIdMissingUsernameSucceeded,
};

}  // namespace

// static
const char* CloudPolicyValidatorBase::StatusToString(Status status) {
  switch (status) {
    case VALIDATION_OK:
      return "OK";
    case VALIDATION_BAD_INITIAL_SIGNATURE:
      return "BAD_INITIAL_SIGNATURE";
    case VALIDATION_BAD_SIGNATURE:
      return "BAD_SIGNATURE";
    case VALIDATION_ERROR_CODE_PRESENT:
      return "ERROR_CODE_PRESENT";
    case VALIDATION_PAYLOAD_PARSE_ERROR:
      return "PAYLOAD_PARSE_ERROR";
    case VALIDATION_WRONG_POLICY_TYPE:
      return "WRONG_POLICY_TYPE";
    case VALIDATION_WRONG_SETTINGS_ENTITY_ID:
      return "WRONG_SETTINGS_ENTITY_ID";
    case VALIDATION_BAD_TIMESTAMP:
      return "BAD_TIMESTAMP";
    case VALIDATION_BAD_DM_TOKEN:
      return "BAD_DM_TOKEN";
    case VALIDATION_BAD_DEVICE_ID:
      return "BAD_DEVICE_ID";
    case VALIDATION_BAD_USER:
      return "BAD_USER";
    case VALIDATION_POLICY_PARSE_ERROR:
      return "POLICY_PARSE_ERROR";
    case VALIDATION_BAD_KEY_VERIFICATION_SIGNATURE:
      return "BAD_KEY_VERIFICATION_SIGNATURE";
    case VALIDATION_VALUE_WARNING:
      return "VALUE_WARNING";
    case VALIDATION_VALUE_ERROR:
      return "VALUE_ERROR";
    case VALIDATION_STATUS_SIZE:
      return "UNKNOWN";
  }
  return "UNKNOWN";
}

CloudPolicyValidatorBase::ValidationResult::ValidationResult() = default;
CloudPolicyValidatorBase::ValidationResult::~ValidationResult() = default;

CloudPolicyValidatorBase::~CloudPolicyValidatorBase() {}

std::unique_ptr<CloudPolicyValidatorBase::ValidationResult>
CloudPolicyValidatorBase::GetValidationResult() const {
  std::unique_ptr<ValidationResult> result =
      std::make_unique<ValidationResult>();
  result->status = status_;
  result->value_validation_issues = value_validation_issues_;
  result->policy_token = policy_data_->policy_token();
  result->policy_data_signature = policy_->policy_data_signature();
  return result;
}

void CloudPolicyValidatorBase::ValidateTimestamp(
    base::Time not_before,
    ValidateTimestampOption timestamp_option) {
  validation_flags_ |= VALIDATE_TIMESTAMP;
  timestamp_not_before_ = not_before.ToJavaTime();
  timestamp_option_ = timestamp_option;
}

void CloudPolicyValidatorBase::ValidateUser(const AccountId& account_id) {
  validation_flags_ |= VALIDATE_USER;
  account_id_ = account_id;
  // Always canonicalize when falls back to username check,
  // because it checks only for regular users.
  canonicalize_user_ = true;
}

void CloudPolicyValidatorBase::ValidateUsername(
    const std::string& expected_user,
    bool canonicalize) {
  validation_flags_ |= VALIDATE_USER;
  account_id_ = AccountId::FromUserEmail(expected_user);
  canonicalize_user_ = canonicalize;
}

void CloudPolicyValidatorBase::ValidateDomain(
    const std::string& expected_domain) {
  validation_flags_ |= VALIDATE_DOMAIN;
  domain_ = gaia::CanonicalizeDomain(expected_domain);
}

void CloudPolicyValidatorBase::ValidateDMToken(
    const std::string& expected_dm_token,
    ValidateDMTokenOption dm_token_option) {
  validation_flags_ |= VALIDATE_DM_TOKEN;
  dm_token_ = expected_dm_token;
  dm_token_option_ = dm_token_option;
}

void CloudPolicyValidatorBase::ValidateDeviceId(
    const std::string& expected_device_id,
    ValidateDeviceIdOption device_id_option) {
  validation_flags_ |= VALIDATE_DEVICE_ID;
  device_id_ = expected_device_id;
  device_id_option_ = device_id_option;
}

void CloudPolicyValidatorBase::ValidatePolicyType(
    const std::string& policy_type) {
  validation_flags_ |= VALIDATE_POLICY_TYPE;
  policy_type_ = policy_type;
}

void CloudPolicyValidatorBase::ValidateSettingsEntityId(
    const std::string& settings_entity_id) {
  validation_flags_ |= VALIDATE_ENTITY_ID;
  settings_entity_id_ = settings_entity_id;
}

void CloudPolicyValidatorBase::ValidatePayload() {
  validation_flags_ |= VALIDATE_PAYLOAD;
}

void CloudPolicyValidatorBase::ValidateCachedKey(
    const std::string& cached_key,
    const std::string& cached_key_signature,
    const std::string& owning_domain) {
  validation_flags_ |= VALIDATE_CACHED_KEY;
  set_owning_domain(owning_domain);
  cached_key_ = cached_key;
  cached_key_signature_ = cached_key_signature;
}

void CloudPolicyValidatorBase::ValidateSignature(const std::string& key) {
  validation_flags_ |= VALIDATE_SIGNATURE;
  DCHECK(key_.empty() || key_ == key);
  key_ = key;
}

void CloudPolicyValidatorBase::ValidateSignatureAllowingRotation(
    const std::string& key,
    const std::string& owning_domain) {
  validation_flags_ |= VALIDATE_SIGNATURE;
  DCHECK(key_.empty() || key_ == key);
  key_ = key;
  set_owning_domain(owning_domain);
  allow_key_rotation_ = true;
}

void CloudPolicyValidatorBase::ValidateInitialKey(
    const std::string& owning_domain) {
  validation_flags_ |= VALIDATE_INITIAL_KEY;
  set_owning_domain(owning_domain);
}

void CloudPolicyValidatorBase::ValidateAgainstCurrentPolicy(
    const em::PolicyData* policy_data,
    ValidateTimestampOption timestamp_option,
    ValidateDMTokenOption dm_token_option,
    ValidateDeviceIdOption device_id_option) {
  base::Time last_policy_timestamp;
  std::string expected_dm_token;
  std::string expected_device_id;
  if (policy_data) {
    last_policy_timestamp = base::Time::FromJavaTime(policy_data->timestamp());
    expected_dm_token = policy_data->request_token();
    expected_device_id = policy_data->device_id();
  }
  ValidateTimestamp(last_policy_timestamp, timestamp_option);
  ValidateDMToken(expected_dm_token, dm_token_option);
  ValidateDeviceId(expected_device_id, device_id_option);
}

CloudPolicyValidatorBase::CloudPolicyValidatorBase(
    std::unique_ptr<em::PolicyFetchResponse> policy_response,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : validation_flags_(0),
      status_(VALIDATION_OK),
      policy_(std::move(policy_response)),
      timestamp_not_before_(0),
      timestamp_option_(TIMESTAMP_VALIDATED),
      dm_token_option_(DM_TOKEN_REQUIRED),
      device_id_option_(DEVICE_ID_REQUIRED),
      canonicalize_user_(false),
      verification_key_(GetPolicyVerificationKey()),
      allow_key_rotation_(false),
      background_task_runner_(background_task_runner) {
  DCHECK(!verification_key_.empty());
}

// static
void CloudPolicyValidatorBase::PostValidationTask(
    std::unique_ptr<CloudPolicyValidatorBase> validator,
    const base::Closure& completion_callback) {
  const auto task_runner = validator->background_task_runner_;
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&CloudPolicyValidatorBase::PerformValidation,
                     std::move(validator), base::ThreadTaskRunnerHandle::Get(),
                     completion_callback));
}

// static
void CloudPolicyValidatorBase::PerformValidation(
    std::unique_ptr<CloudPolicyValidatorBase> self,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::Closure& completion_callback) {
  // Run the validation activities on this thread.
  self->RunValidation();

  // Report completion on |task_runner|.
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&CloudPolicyValidatorBase::ReportCompletion,
                                std::move(self), completion_callback));
}

// static
void CloudPolicyValidatorBase::ReportCompletion(
    std::unique_ptr<CloudPolicyValidatorBase> self,
    const base::Closure& completion_callback) {
  completion_callback.Run();
}

void CloudPolicyValidatorBase::RunValidation() {
  policy_data_.reset(new em::PolicyData());
  RunChecks();
}

CloudPolicyValidatorBase::Status CloudPolicyValidatorBase::CheckProtoPayload(
    google::protobuf::MessageLite* payload) {
  DCHECK(payload);
  if (!policy_data_ || !policy_data_->has_policy_value() ||
      !payload->ParseFromString(policy_data_->policy_value()) ||
      !payload->IsInitialized()) {
    LOG(ERROR) << "Failed to decode policy payload protobuf";
    return VALIDATION_POLICY_PARSE_ERROR;
  }
  return VALIDATION_OK;
}

void CloudPolicyValidatorBase::RunChecks() {
  status_ = VALIDATION_OK;
  if ((policy_->has_error_code() && policy_->error_code() != 200) ||
      (policy_->has_error_message() && !policy_->error_message().empty())) {
    LOG(ERROR) << "Error in policy blob."
               << " code: " << policy_->error_code()
               << " message: " << policy_->error_message();
    status_ = VALIDATION_ERROR_CODE_PRESENT;
    return;
  }

  // Parse policy data.
  if (!policy_data_->ParseFromString(policy_->policy_data()) ||
      !policy_data_->IsInitialized()) {
    LOG(ERROR) << "Failed to parse policy response";
    status_ = VALIDATION_PAYLOAD_PARSE_ERROR;
    return;
  }

  // Table of checks we run. These are sorted by descending severity of the
  // error, s.t. the most severe check will determine the validation status.
  static const struct {
    int flag;
    Status (CloudPolicyValidatorBase::*checkFunction)();
  } kCheckFunctions[] = {
      {VALIDATE_SIGNATURE, &CloudPolicyValidatorBase::CheckSignature},
      {VALIDATE_INITIAL_KEY, &CloudPolicyValidatorBase::CheckInitialKey},
      {VALIDATE_CACHED_KEY, &CloudPolicyValidatorBase::CheckCachedKey},
      {VALIDATE_POLICY_TYPE, &CloudPolicyValidatorBase::CheckPolicyType},
      {VALIDATE_ENTITY_ID, &CloudPolicyValidatorBase::CheckEntityId},
      {VALIDATE_DM_TOKEN, &CloudPolicyValidatorBase::CheckDMToken},
      {VALIDATE_DEVICE_ID, &CloudPolicyValidatorBase::CheckDeviceId},
      {VALIDATE_USER, &CloudPolicyValidatorBase::CheckUser},
      {VALIDATE_DOMAIN, &CloudPolicyValidatorBase::CheckDomain},
      {VALIDATE_TIMESTAMP, &CloudPolicyValidatorBase::CheckTimestamp},
      {VALIDATE_PAYLOAD, &CloudPolicyValidatorBase::CheckPayload},
      {VALIDATE_VALUES, &CloudPolicyValidatorBase::CheckValues},
  };

  for (size_t i = 0; i < arraysize(kCheckFunctions); ++i) {
    if (validation_flags_ & kCheckFunctions[i].flag) {
      status_ = (this->*(kCheckFunctions[i].checkFunction))();
      if (status_ != VALIDATION_OK)
        break;
    }
  }
}

// Verifies the |new_public_key_verification_signature_deprecated| for the
// |new_public_key| in the policy blob.
bool CloudPolicyValidatorBase::CheckNewPublicKeyVerificationSignature() {
  if (!policy_->has_new_public_key_verification_signature_deprecated()) {
    // Policy does not contain a verification signature, so log an error.
    LOG(ERROR) << "Policy is missing public_key_verification_signature";
    UMA_HISTOGRAM_ENUMERATION(kMetricPolicyKeyVerification,
                              METRIC_POLICY_KEY_VERIFICATION_SIGNATURE_MISSING,
                              METRIC_POLICY_KEY_VERIFICATION_SIZE);
    return false;
  }

  if (!CheckVerificationKeySignature(
          policy_->new_public_key(), verification_key_,
          policy_->new_public_key_verification_signature_deprecated())) {
    LOG(ERROR) << "Signature verification failed";
    UMA_HISTOGRAM_ENUMERATION(kMetricPolicyKeyVerification,
                              METRIC_POLICY_KEY_VERIFICATION_FAILED,
                              METRIC_POLICY_KEY_VERIFICATION_SIZE);
    return false;
  }
  // Signature verification succeeded - return success to the caller.
  DVLOG(1) << "Signature verification succeeded";
  UMA_HISTOGRAM_ENUMERATION(kMetricPolicyKeyVerification,
                            METRIC_POLICY_KEY_VERIFICATION_SUCCEEDED,
                            METRIC_POLICY_KEY_VERIFICATION_SIZE);
  return true;
}

bool CloudPolicyValidatorBase::CheckVerificationKeySignature(
    const std::string& key,
    const std::string& verification_key,
    const std::string& signature) {
  DCHECK(!verification_key.empty());
  em::DEPRECATEDPolicyPublicKeyAndDomain signed_data;
  signed_data.set_new_public_key(key);

  // If no owning_domain_ supplied, try extracting the domain from the policy
  // itself (this happens on certain platforms during startup, when we validate
  // cached policy before prefs are loaded).
  std::string domain =
      owning_domain_.empty() ? ExtractDomainFromPolicy() : owning_domain_;
  if (domain.empty()) {
    LOG(ERROR) << "Policy does not contain a domain";
    return false;
  }
  signed_data.set_domain(domain);
  std::string signed_data_as_string;
  if (!signed_data.SerializeToString(&signed_data_as_string)) {
    DLOG(ERROR) << "Could not serialize verification key to string";
    return false;
  }
  return VerifySignature(signed_data_as_string, verification_key, signature,
                         SHA256);
}

std::string CloudPolicyValidatorBase::ExtractDomainFromPolicy() {
  std::string domain;
  if (policy_data_->has_username()) {
    domain = gaia::ExtractDomainName(
        gaia::CanonicalizeEmail(gaia::SanitizeEmail(policy_data_->username())));
  }
  return domain;
}

void CloudPolicyValidatorBase::set_owning_domain(
    const std::string& owning_domain) {
  // Make sure we aren't overwriting the owning domain with a different one.
  DCHECK(owning_domain_.empty() || owning_domain_ == owning_domain);
  owning_domain_ = owning_domain;
}

CloudPolicyValidatorBase::Status CloudPolicyValidatorBase::CheckSignature() {
  const std::string* signature_key = &key_;
  if (policy_->has_new_public_key() && allow_key_rotation_) {
    signature_key = &policy_->new_public_key();
    if (!policy_->has_new_public_key_signature() ||
        !VerifySignature(policy_->new_public_key(), key_,
                         policy_->new_public_key_signature(), SHA1)) {
      LOG(ERROR) << "New public key rotation signature verification failed";
      return VALIDATION_BAD_SIGNATURE;
    }

    if (!CheckNewPublicKeyVerificationSignature()) {
      LOG(ERROR) << "New public key root verification failed";
      return VALIDATION_BAD_KEY_VERIFICATION_SIGNATURE;
    }
  }

  if (!policy_->has_policy_data_signature() ||
      !VerifySignature(policy_->policy_data(), *signature_key,
                       policy_->policy_data_signature(), SHA1)) {
    LOG(ERROR) << "Policy signature validation failed";
    return VALIDATION_BAD_SIGNATURE;
  }

  return VALIDATION_OK;
}

CloudPolicyValidatorBase::Status CloudPolicyValidatorBase::CheckInitialKey() {
  if (!policy_->has_new_public_key() || !policy_->has_policy_data_signature() ||
      !VerifySignature(policy_->policy_data(), policy_->new_public_key(),
                       policy_->policy_data_signature(), SHA1)) {
    LOG(ERROR) << "Initial policy signature validation failed";
    return VALIDATION_BAD_INITIAL_SIGNATURE;
  }

  if (!CheckNewPublicKeyVerificationSignature()) {
    LOG(ERROR) << "Initial policy root signature validation failed";
    return VALIDATION_BAD_KEY_VERIFICATION_SIGNATURE;
  }
  return VALIDATION_OK;
}

CloudPolicyValidatorBase::Status CloudPolicyValidatorBase::CheckCachedKey() {
  if (!CheckVerificationKeySignature(cached_key_, verification_key_,
                                     cached_key_signature_)) {
    LOG(ERROR) << "Cached key signature verification failed";
    return VALIDATION_BAD_KEY_VERIFICATION_SIGNATURE;
  } else {
    DVLOG(1) << "Cached key signature verification succeeded";
  }
  return VALIDATION_OK;
}

CloudPolicyValidatorBase::Status CloudPolicyValidatorBase::CheckPolicyType() {
  if (!policy_data_->has_policy_type() ||
      policy_data_->policy_type() != policy_type_) {
    LOG(ERROR) << "Wrong policy type " << policy_data_->policy_type();
    return VALIDATION_WRONG_POLICY_TYPE;
  }

  return VALIDATION_OK;
}

CloudPolicyValidatorBase::Status CloudPolicyValidatorBase::CheckEntityId() {
  if (!policy_data_->has_settings_entity_id() ||
      policy_data_->settings_entity_id() != settings_entity_id_) {
    LOG(ERROR) << "Wrong settings_entity_id "
               << policy_data_->settings_entity_id() << ", expected "
               << settings_entity_id_;
    return VALIDATION_WRONG_SETTINGS_ENTITY_ID;
  }

  return VALIDATION_OK;
}

CloudPolicyValidatorBase::Status CloudPolicyValidatorBase::CheckTimestamp() {
  if (timestamp_option_ == TIMESTAMP_NOT_VALIDATED)
    return VALIDATION_OK;

  if (!policy_data_->has_timestamp()) {
    LOG(ERROR) << "Policy timestamp missing";
    return VALIDATION_BAD_TIMESTAMP;
  }

  if (policy_data_->timestamp() < timestamp_not_before_) {
    LOG(ERROR) << "Policy too old: " << policy_data_->timestamp();
    return VALIDATION_BAD_TIMESTAMP;
  }

  return VALIDATION_OK;
}

CloudPolicyValidatorBase::Status CloudPolicyValidatorBase::CheckDMToken() {
  if (dm_token_option_ == DM_TOKEN_REQUIRED &&
      (!policy_data_->has_request_token() ||
       policy_data_->request_token().empty())) {
    LOG(ERROR) << "Empty DM token encountered - expected: " << dm_token_;
    return VALIDATION_BAD_DM_TOKEN;
  }
  if (!dm_token_.empty() && policy_data_->request_token() != dm_token_) {
    LOG(ERROR) << "Invalid DM token: " << policy_data_->request_token()
               << " - expected: " << dm_token_;
    return VALIDATION_BAD_DM_TOKEN;
  }

  return VALIDATION_OK;
}

CloudPolicyValidatorBase::Status CloudPolicyValidatorBase::CheckDeviceId() {
  if (device_id_option_ == DEVICE_ID_REQUIRED &&
      (!policy_data_->has_device_id() || policy_data_->device_id().empty())) {
    LOG(ERROR) << "Empty device id encountered - expected: " << device_id_;
    return VALIDATION_BAD_DEVICE_ID;
  }
  if (!device_id_.empty() && policy_data_->device_id() != device_id_) {
    LOG(ERROR) << "Invalid device id: " << policy_data_->device_id()
               << " - expected: " << device_id_;
    return VALIDATION_BAD_DEVICE_ID;
  }
  return VALIDATION_OK;
}

CloudPolicyValidatorBase::Status CloudPolicyValidatorBase::CheckUser() {
  if (!policy_data_->has_username() && !policy_data_->has_gaia_id()) {
    LOG(ERROR) << "Policy is missing user name and gaia id";
    return VALIDATION_BAD_USER;
  }

  if (policy_data_->has_gaia_id() && !policy_data_->gaia_id().empty() &&
      account_id_.GetAccountType() == AccountType::GOOGLE &&
      !account_id_.GetGaiaId().empty()) {
    std::string expected = account_id_.GetGaiaId();
    std::string actual = policy_data_->gaia_id();

    if (expected != actual) {
      LOG(ERROR) << "Invalid gaia id: " << actual;
      UMA_HISTOGRAM_ENUMERATION(kMetricPolicyUserVerification,
                                MetricPolicyUserVerification::kGaiaIdFailed);
      return VALIDATION_BAD_USER;
    }
    UMA_HISTOGRAM_ENUMERATION(kMetricPolicyUserVerification,
                              MetricPolicyUserVerification::kGaiaIdSucceeded);
  } else {
    std::string expected = account_id_.GetUserEmail();
    std::string actual = policy_data_->username();
    if (canonicalize_user_) {
      expected = gaia::CanonicalizeEmail(gaia::SanitizeEmail(expected));
      actual = gaia::CanonicalizeEmail(gaia::SanitizeEmail(actual));
    }

    if (expected != actual) {
      LOG(ERROR) << "Invalid user name " << policy_data_->username();
      UMA_HISTOGRAM_ENUMERATION(kMetricPolicyUserVerification,
                                MetricPolicyUserVerification::kUsernameFailed);
      return VALIDATION_BAD_USER;
    }
    if (account_id_.GetAccountType() != AccountType::GOOGLE ||
        account_id_.GetGaiaId().empty()) {
      UMA_HISTOGRAM_ENUMERATION(
          kMetricPolicyUserVerification,
          MetricPolicyUserVerification::kUsernameSucceeded);
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          kMetricPolicyUserVerification,
          MetricPolicyUserVerification::kGaiaIdMissingUsernameSucceeded);
    }
  }

  return VALIDATION_OK;
}

CloudPolicyValidatorBase::Status CloudPolicyValidatorBase::CheckDomain() {
  std::string policy_domain = ExtractDomainFromPolicy();
  if (policy_domain.empty()) {
    LOG(ERROR) << "Policy is missing user name";
    return VALIDATION_BAD_USER;
  }

  if (domain_ != policy_domain) {
    LOG(ERROR) << "Invalid user name " << policy_data_->username();
    return VALIDATION_BAD_USER;
  }

  return VALIDATION_OK;
}

// static
bool CloudPolicyValidatorBase::VerifySignature(const std::string& data,
                                               const std::string& key,
                                               const std::string& signature,
                                               SignatureType signature_type) {
  crypto::SignatureVerifier verifier;
  crypto::SignatureVerifier::SignatureAlgorithm algorithm;
  switch (signature_type) {
    case SHA1:
      algorithm = crypto::SignatureVerifier::RSA_PKCS1_SHA1;
      break;
    case SHA256:
      algorithm = crypto::SignatureVerifier::RSA_PKCS1_SHA256;
      break;
    default:
      NOTREACHED() << "Invalid signature type: " << signature_type;
      return false;
  }

  if (!verifier.VerifyInit(algorithm,
                           base::as_bytes(base::make_span(signature)),
                           base::as_bytes(base::make_span(key)))) {
    DLOG(ERROR) << "Invalid verification signature/key format";
    return false;
  }
  verifier.VerifyUpdate(base::as_bytes(base::make_span(data)));
  return verifier.VerifyFinal();
}

template class CloudPolicyValidator<em::CloudPolicySettings>;

#if !defined(OS_ANDROID) && !defined(OS_IOS)
template class CloudPolicyValidator<em::ExternalPolicyData>;
#endif

}  // namespace policy
