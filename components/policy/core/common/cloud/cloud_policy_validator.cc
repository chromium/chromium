// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/policy/core/common/cloud/cloud_policy_validator.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cloud_policy_validator.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/signature_verifier.h"
#include "google_apis/gaia/gaia_auth_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/system/sys_info.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace em = enterprise_management;

namespace policy {

namespace {

const char kMetricPolicyUserVerification[] =
    "Enterprise.PolicyUserVerification";
const char kMetricKeySignatureVerification[] =
    "Enterprise.KeySignatureVerification";

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

enum class MetricKeySignatureVerification {
  // New key signature verification success.
  kSuccess = 0,
  // Both signatures for the new key failed to verify.
  kFailed = 1,
  // Failed to verify the new signature but succeeded to verify the old
  // signature.
  kDeprecatedSuccess = 2,
  kMaxValue = kDeprecatedSuccess,
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

CloudPolicyValidatorBase::~CloudPolicyValidatorBase() = default;

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
  timestamp_not_before_ = not_before.InMillisecondsSinceUnixEpoch();
  timestamp_option_ = timestamp_option;
}

void CloudPolicyValidatorBase::ValidateUser(const AccountId& account_id) {
  validation_flags_ |= VALIDATE_USER;
  username_ = account_id.GetUserEmail();
  gaia_id_ = account_id.GetGaiaId();
  // Always canonicalize when falls back to username check,
  // because it checks only for regular users.
  canonicalize_user_ = true;
}

void CloudPolicyValidatorBase::ValidateUsernameAndGaiaId(
    const std::string& expected_user,
    const std::string& gaia_id) {
  validation_flags_ |= VALIDATE_USER;
  username_ = expected_user;
  gaia_id_ = gaia_id;
  canonicalize_user_ = false;
}

void CloudPolicyValidatorBase::ValidateUsername(
    const std::string& expected_user) {
  validation_flags_ |= VALIDATE_USER;
  username_ = expected_user;
  gaia_id_.clear();
  canonicalize_user_ = false;
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
    const std::string& owning_domain,
    const std::string& new_public_key_verification_data,
    const std::string& new_public_key_verification_data_signature) {
  validation_flags_ |= VALIDATE_CACHED_KEY;
  set_owning_domain(owning_domain);
  cached_key_ = cached_key;
  cached_key_signature_ = cached_key_signature;
  new_cached_key_ = new_public_key_verification_data;
  new_cached_key_signature_ = new_public_key_verification_data_signature;
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
    last_policy_timestamp =
        base::Time::FromMillisecondsSinceUnixEpoch(policy_data->timestamp());
    expected_dm_token = policy_data->request_token();
    expected_device_id = policy_data->device_id();
  }
  ValidateTimestamp(last_policy_timestamp, timestamp_option);
  ValidateDMToken(expected_dm_token, dm_token_option);
  ValidateDeviceId(expected_device_id, device_id_option);
}

// static
bool CloudPolicyValidatorBase::VerifySignature(const std::string& data,
                                               const std::string& key,
                                               const std::string& signature,
                                               SignatureType signature_type) {
  crypto::SignatureVerifier verifier;
  crypto::SignatureVerifier::SignatureAlgorithm algorithm;
  switch (signature_type) {
    case em::PolicyFetchRequest::SHA1_RSA:
      algorithm = crypto::SignatureVerifier::RSA_PKCS1_SHA1;
      break;
    case em::PolicyFetchRequest::SHA256_RSA:
      algorithm = crypto::SignatureVerifier::RSA_PKCS1_SHA256;
      break;
    default:
      // Treat `em::PolicyFetchRequest::NONE` as unsigned blobs, which is
      // not supported.
      LOG(ERROR) << "Invalid signature type for verification: "
                 << signature_type;
      return false;
  }

  if (!verifier.VerifyInit(algorithm,
                           base::as_bytes(base::make_span(signature)),
                           base::as_bytes(base::make_span(key)))) {
    DLOG_POLICY(ERROR, CBCM_ENROLLMENT)
        << "Invalid verification signature/key format";
    return false;
  }
  verifier.VerifyUpdate(base::as_bytes(base::make_span(data)));
  return verifier.VerifyFinal();
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
      verification_key_(GetCurrentPolicyVerificationKey()),
      allow_key_rotation_(false),
      background_task_runner_(background_task_runner) {}

// static
std::optional<std::string>
CloudPolicyValidatorBase::GetCurrentPolicyVerificationKey() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Empty `verification_key_` is only allowed on Chrome OS test image when
  // policy key verification is disabled via command line flag.
  if (command_line->HasSwitch(switches::kDisablePolicyKeyVerification)) {
    base::SysInfo::CrashIfChromeOSNonTestImage();
    // GetPolicyVerificationKey() returns a non-empty string.
    return std::nullopt;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  if (command_line->HasSwitch(switches::kPolicyVerificationKey)) {
    CHECK_IS_TEST();
    std::string decoded_key;
    CHECK(base::Base64Decode(
        command_line->GetSwitchValueASCII(switches::kPolicyVerificationKey),
        &decoded_key));
    return decoded_key;
  }
  return GetPolicyVerificationKey();
}

// static
void CloudPolicyValidatorBase::PostValidationTask(
    std::unique_ptr<CloudPolicyValidatorBase> validator,
    base::OnceClosure completion_callback) {
  const auto task_runner = validator->background_task_runner_;
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&CloudPolicyValidatorBase::PerformValidation,
                     std::move(validator),
                     base::SingleThreadTaskRunner::GetCurrentDefault(),
                     std::move(completion_callback)));
}

// static
void CloudPolicyValidatorBase::PerformValidation(
    std::unique_ptr<CloudPolicyValidatorBase> self,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::OnceClosure completion_callback) {
  // Run the validation activities on this thread.
  self->RunValidation();

  // Report completion on |task_runner|.
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&CloudPolicyValidatorBase::ReportCompletion,
                     std::move(self), std::move(completion_callback)));
}

// static
void CloudPolicyValidatorBase::ReportCompletion(
    std::unique_ptr<CloudPolicyValidatorBase> self,
    base::OnceClosure completion_callback) {
  std::move(completion_callback).Run();
}

void CloudPolicyValidatorBase::RunValidation() {
  policy_data_ = std::make_unique<em::PolicyData>();
  RunChecks();
}

CloudPolicyValidatorBase::Status CloudPolicyValidatorBase::CheckProtoPayload(
    google::protobuf::MessageLite* payload) {
  DCHECK(payload);
  if (!policy_data_ || !policy_data_->has_policy_value() ||
      !payload->ParseFromString(policy_data_->policy_value()) ||
      !payload->IsInitialized()) {
    LOG_POLICY(ERROR, POLICY_FETCHING)
        << "Failed to decode policy payload protobuf";
    return VALIDATION_POLICY_PARSE_ERROR;
  }
  return VALIDATION_OK;
}

void CloudPolicyValidatorBase::RunChecks() {
  status_ = VALIDATION_OK;
  if ((policy_->has_error_code() && policy_->error_code() != 200) ||
      (policy_->has_error_message() && !policy_->error_message().empty())) {
    LOG_POLICY(ERROR, POLICY_FETCHING)
        << "Error in policy blob."
        << " code: " << policy_->error_code()
        << " message: " << policy_->error_message();
    status_ = VALIDATION_ERROR_CODE_PRESENT;
    return;
  }

  // Parse policy data.
  if (!policy_data_->ParseFromString(policy_->policy_data()) ||
      !policy_data_->IsInitialized()) {
    LOG_POLICY(ERROR, POLICY_FETCHING) << "Failed to parse policy response";
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

  for (size_t i = 0; i < std::size(kCheckFunctions); ++i) {
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Skip verification if the key is empty (disabled via command line).
  if (!verification_key_) {
    return true;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  if (policy_->has_new_public_key_verification_data() &&
      policy_->has_new_public_key_verification_data_signature() &&
      VerifySignature(policy_->new_public_key_verification_data(),
                      verification_key_.value(),
                      policy_->new_public_key_verification_data_signature(),
                      em::PolicyFetchRequest::SHA256_RSA) &&
      CheckDomainInPublicKeyVerificationData(
          policy_->new_public_key_verification_data())) {
    UMA_HISTOGRAM_ENUMERATION(kMetricKeySignatureVerification,
                              MetricKeySignatureVerification::kSuccess);
    // Signature verification succeeded - return success to the caller.
    DVLOG(1) << "Signature verification succeeded";
    return true;
  }
  LOG(ERROR) << "Signature verification failed, has data: "
             << policy_->has_new_public_key_verification_data();

  // Fallback to the deprecated signature to check if that works.
  // TODO(b/314810831): Remove the deprecated part when the UMA confirms the new
  // verification works.
  if (!policy_->has_new_public_key_verification_signature_deprecated()) {
    UMA_HISTOGRAM_ENUMERATION(kMetricKeySignatureVerification,
                              MetricKeySignatureVerification::kFailed);
    // Policy does not contain a verification signature, so log an error.
    LOG_POLICY(ERROR, POLICY_FETCHING)
        << "Policy is missing public_key_verification_signature";
    return false;
  }

  if (!CheckVerificationKeySignatureDeprecated(
          policy_->new_public_key(), verification_key_.value(),
          policy_->new_public_key_verification_signature_deprecated())) {
    UMA_HISTOGRAM_ENUMERATION(kMetricKeySignatureVerification,
                              MetricKeySignatureVerification::kFailed);
    LOG_POLICY(ERROR, POLICY_FETCHING) << "Signature verification failed";
    return false;
  }

  UMA_HISTOGRAM_ENUMERATION(kMetricKeySignatureVerification,
                            MetricKeySignatureVerification::kDeprecatedSuccess);
  // Signature verification succeeded - return success to the caller.
  DVLOG(1) << "Deprecated signature verification succeeded";
  return true;
}

bool CloudPolicyValidatorBase::CheckVerificationKeySignatureDeprecated(
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
    LOG_POLICY(ERROR, POLICY_FETCHING) << "Policy does not contain a domain";
    return false;
  }
  signed_data.set_domain(domain);
  std::string signed_data_as_string;
  if (!signed_data.SerializeToString(&signed_data_as_string)) {
    DLOG_POLICY(ERROR, POLICY_FETCHING)
        << "Could not serialize verification key to string";
    return false;
  }
  return VerifySignature(signed_data_as_string, verification_key, signature,
                         em::PolicyFetchRequest::SHA256_RSA);
}

std::string CloudPolicyValidatorBase::ExtractDomainFromPolicy() {
  std::string domain;
  if (policy_data_->has_username()) {
    domain = gaia::ExtractDomainName(
        gaia::CanonicalizeEmail(gaia::SanitizeEmail(policy_data_->username())));
  }
  return domain;
}

bool CloudPolicyValidatorBase::CheckDomainInPublicKeyVerificationData(
    const std::string& new_public_key_verification_data) {
  em::PublicKeyVerificationData public_key_data;
  if (!public_key_data.ParseFromString(new_public_key_verification_data)) {
    LOG_POLICY(ERROR, POLICY_FETCHING)
        << "Failed to deserialize new public key.";
    return false;
  }
  if (public_key_data.domain() != ExtractDomainFromPolicy()) {
    LOG_POLICY(ERROR, POLICY_FETCHING) << "Domain mismatch in new public key.";
    return false;
  }
  return true;
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
                         policy_->new_public_key_signature(),
                         GetSignatureType())) {
      LOG_POLICY(ERROR, POLICY_FETCHING)
          << "New public key rotation signature verification failed";
      return VALIDATION_BAD_SIGNATURE;
    }

    if (!CheckNewPublicKeyVerificationSignature()) {
      LOG_POLICY(ERROR, POLICY_FETCHING)
          << "New public key root verification failed";
      return VALIDATION_BAD_KEY_VERIFICATION_SIGNATURE;
    }
  }

  if (!policy_->has_policy_data_signature() ||
      !VerifySignature(policy_->policy_data(), *signature_key,
                       policy_->policy_data_signature(), GetSignatureType())) {
    LOG_POLICY(ERROR, POLICY_FETCHING) << "Policy signature validation failed";
    return VALIDATION_BAD_SIGNATURE;
  }

  return VALIDATION_OK;
}

CloudPolicyValidatorBase::Status CloudPolicyValidatorBase::CheckInitialKey() {
  if (!policy_->has_new_public_key() || !policy_->has_policy_data_signature() ||
      !VerifySignature(policy_->policy_data(), policy_->new_public_key(),
                       policy_->policy_data_signature(), GetSignatureType())) {
    LOG_POLICY(ERROR, POLICY_FETCHING)
        << "Initial policy signature validation failed";
    return VALIDATION_BAD_INITIAL_SIGNATURE;
  }

  if (!CheckNewPublicKeyVerificationSignature()) {
    LOG_POLICY(ERROR, POLICY_FETCHING)
        << "Initial policy root signature validation failed";
    return VALIDATION_BAD_KEY_VERIFICATION_SIGNATURE;
  }
  return VALIDATION_OK;
}

CloudPolicyValidatorBase::Status CloudPolicyValidatorBase::CheckCachedKey() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Skip verification if the key is empty (disabled via command line).
  if (!verification_key_) {
    return VALIDATION_OK;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  if (VerifySignature(new_cached_key_, verification_key_.value(),
                      new_cached_key_signature_,
                      em::PolicyFetchRequest::SHA256_RSA) &&
      CheckDomainInPublicKeyVerificationData(new_cached_key_)) {
    UMA_HISTOGRAM_ENUMERATION(kMetricKeySignatureVerification,
                              MetricKeySignatureVerification::kSuccess);
    // Signature verification succeeded - return success to the caller.
    DVLOG_POLICY(1, POLICY_FETCHING) << "Signature verification succeeded";
    return VALIDATION_OK;
  }

  LOG_POLICY(ERROR, POLICY_FETCHING) << "New signature verification failed";

  if (!CheckVerificationKeySignatureDeprecated(
          cached_key_, verification_key_.value(), cached_key_signature_)) {
    LOG_POLICY(ERROR, POLICY_FETCHING)
        << "Cached key signature verification failed";
    UMA_HISTOGRAM_ENUMERATION(kMetricKeySignatureVerification,
                              MetricKeySignatureVerification::kFailed);
    return VALIDATION_BAD_KEY_VERIFICATION_SIGNATURE;
  } else {
    DVLOG_POLICY(1, POLICY_FETCHING)
        << "Cached key signature verification succeeded";
  }
  UMA_HISTOGRAM_ENUMERATION(kMetricKeySignatureVerification,
                            MetricKeySignatureVerification::kDeprecatedSuccess);
  return VALIDATION_OK;
}

CloudPolicyValidatorBase::Status CloudPolicyValidatorBase::CheckPolicyType() {
  if (!policy_data_->has_policy_type() ||
      policy_data_->policy_type() != policy_type_) {
    LOG_POLICY(ERROR, POLICY_FETCHING)
        << "Wrong policy type " << policy_data_->policy_type();
    return VALIDATION_WRONG_POLICY_TYPE;
  }

  return VALIDATION_OK;
}

CloudPolicyValidatorBase::Status CloudPolicyValidatorBase::CheckEntityId() {
  if (!policy_data_->has_settings_entity_id() ||
      policy_data_->settings_entity_id() != settings_entity_id_) {
    LOG_POLICY(ERROR, POLICY_FETCHING)
        << "Wrong settings_entity_id " << policy_data_->settings_entity_id()
        << ", expected " << settings_entity_id_;
    return VALIDATION_WRONG_SETTINGS_ENTITY_ID;
  }

  return VALIDATION_OK;
}

CloudPolicyValidatorBase::Status CloudPolicyValidatorBase::CheckTimestamp() {
  if (timestamp_option_ == TIMESTAMP_NOT_VALIDATED)
    return VALIDATION_OK;

  if (!policy_data_->has_timestamp()) {
    LOG_POLICY(ERROR, POLICY_FETCHING) << "Policy timestamp missing";
    return VALIDATION_BAD_TIMESTAMP;
  }

  if (policy_data_->timestamp() < timestamp_not_before_) {
    LOG_POLICY(ERROR, POLICY_FETCHING)
        << "Policy too old: " << policy_data_->timestamp();
    return VALIDATION_BAD_TIMESTAMP;
  }

  return VALIDATION_OK;
}

CloudPolicyValidatorBase::Status CloudPolicyValidatorBase::CheckDMToken() {
  if (dm_token_option_ == DM_TOKEN_REQUIRED &&
      (!policy_data_->has_request_token() ||
       policy_data_->request_token().empty())) {
    LOG_POLICY(ERROR, POLICY_FETCHING)
        << "Empty DM token encountered - expected: " << dm_token_;
    return VALIDATION_BAD_DM_TOKEN;
  }
  if (!dm_token_.empty() && policy_data_->request_token() != dm_token_) {
    LOG_POLICY(ERROR, POLICY_FETCHING)
        << "Invalid DM token: " << policy_data_->request_token()
        << " - expected: " << dm_token_;
    return VALIDATION_BAD_DM_TOKEN;
  }

  return VALIDATION_OK;
}

CloudPolicyValidatorBase::Status CloudPolicyValidatorBase::CheckDeviceId() {
  if (device_id_option_ == DEVICE_ID_REQUIRED &&
      (!policy_data_->has_device_id() || policy_data_->device_id().empty())) {
    LOG_POLICY(ERROR, POLICY_FETCHING)
        << "Empty device id encountered - expected: " << device_id_;
    return VALIDATION_BAD_DEVICE_ID;
  }
  if (!device_id_.empty() && policy_data_->device_id() != device_id_) {
    LOG_POLICY(ERROR, POLICY_FETCHING)
        << "Invalid device id: " << policy_data_->device_id()
        << " - expected: " << device_id_;
    return VALIDATION_BAD_DEVICE_ID;
  }
  return VALIDATION_OK;
}

CloudPolicyValidatorBase::Status CloudPolicyValidatorBase::CheckUser() {
  if (!policy_data_->has_username() && !policy_data_->has_gaia_id()) {
    LOG_POLICY(ERROR, POLICY_FETCHING)
        << "Policy is missing user name and gaia id";
    return VALIDATION_BAD_USER;
  }

  if (policy_data_->has_gaia_id() && !policy_data_->gaia_id().empty() &&
      !gaia_id_.empty()) {
    std::string expected = gaia_id_;
    std::string actual = policy_data_->gaia_id();

    if (expected != actual) {
      LOG_POLICY(ERROR, POLICY_FETCHING) << "Invalid gaia id: " << actual;
      UMA_HISTOGRAM_ENUMERATION(kMetricPolicyUserVerification,
                                MetricPolicyUserVerification::kGaiaIdFailed);
      return VALIDATION_BAD_USER;
    }
    UMA_HISTOGRAM_ENUMERATION(kMetricPolicyUserVerification,
                              MetricPolicyUserVerification::kGaiaIdSucceeded);
  } else {
    std::string expected = username_;
    std::string actual = policy_data_->username();
    if (canonicalize_user_) {
      expected = gaia::CanonicalizeEmail(gaia::SanitizeEmail(expected));
      actual = gaia::CanonicalizeEmail(gaia::SanitizeEmail(actual));
    }

    if (expected != actual) {
      LOG_POLICY(ERROR, POLICY_FETCHING)
          << "Invalid user name " << actual << ", expected " << expected;
      UMA_HISTOGRAM_ENUMERATION(kMetricPolicyUserVerification,
                                MetricPolicyUserVerification::kUsernameFailed);
      return VALIDATION_BAD_USER;
    }
    if (gaia_id_.empty()) {
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
    LOG_POLICY(ERROR, POLICY_FETCHING) << "Policy is missing user name";
    return VALIDATION_BAD_USER;
  }

  if (domain_ != policy_domain) {
    LOG_POLICY(ERROR, POLICY_FETCHING)
        << "Invalid domain name " << policy_domain << " - " << domain_;
    return VALIDATION_BAD_USER;
  }

  return VALIDATION_OK;
}

CloudPolicyValidatorBase::SignatureType
CloudPolicyValidatorBase::GetSignatureType() {
  if (!policy_->has_policy_data_signature_type()) {
    return em::PolicyFetchRequest::SHA1_RSA;
  }

  return policy_->policy_data_signature_type();
}

template class CloudPolicyValidator<em::CloudPolicySettings>;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
template class CloudPolicyValidator<em::ExternalPolicyData>;
#endif

}  // namespace policy
