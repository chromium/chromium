// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_validator.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/rsa_private_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/system/sys_info.h"
#include "base/test/scoped_chromeos_version_info.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest-death-test.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace em = enterprise_management;

using testing::Invoke;
using testing::Mock;

namespace policy {

namespace {

ACTION_P(CheckStatus, expected_status) {
  EXPECT_EQ(expected_status, arg0->status());
}

const char kPolicyName[] = "fake-policy-name";
const ValueValidationIssue::Severity kSeverity = ValueValidationIssue::kError;
const char kMessage[] = "fake-message";

class FakeUserPolicyValueValidator
    : public PolicyValueValidator<em::CloudPolicySettings> {
 public:
  bool ValidateValues(
      const enterprise_management::CloudPolicySettings& policy_payload,
      std::vector<ValueValidationIssue>* validation_issues) const override {
    validation_issues->push_back({kPolicyName, kSeverity, kMessage});
    return false;
  }
};

class CloudPolicyValidatorTest : public testing::Test {
 public:
  CloudPolicyValidatorTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI),
        timestamp_(base::Time::FromMillisecondsSinceUnixEpoch(
            PolicyBuilder::kFakeTimestamp)),
        timestamp_option_(CloudPolicyValidatorBase::TIMESTAMP_VALIDATED),
        dm_token_option_(CloudPolicyValidatorBase::DM_TOKEN_REQUIRED),
        device_id_option_(CloudPolicyValidatorBase::DEVICE_ID_REQUIRED),
        allow_key_rotation_(true),
        existing_dm_token_(PolicyBuilder::kFakeToken),
        existing_device_id_(PolicyBuilder::kFakeDeviceId),
        owning_domain_(PolicyBuilder::kFakeDomain),
        cached_key_signature_(PolicyBuilder::GetTestSigningKeySignature()),
        validate_by_gaia_id_(true),
        validate_values_(false) {
    policy_.SetDefaultNewSigningKey();

    // Set the verification key to be used for testing by the
    // CloudPolicyValidator.
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(
        switches::kPolicyVerificationKey,
        PolicyBuilder::GetEncodedPolicyVerificationKey());
  }
  CloudPolicyValidatorTest(const CloudPolicyValidatorTest&) = delete;
  CloudPolicyValidatorTest& operator=(const CloudPolicyValidatorTest&) = delete;

  void Validate(testing::Action<void(UserCloudPolicyValidator*)> check_action) {
    policy_.Build();
    ValidatePolicy(check_action, policy_.GetCopy());
  }

  void ValidatePolicy(
      testing::Action<void(UserCloudPolicyValidator*)> check_action,
      std::unique_ptr<em::PolicyFetchResponse> policy_response) {
    // Create a validator.
    std::unique_ptr<UserCloudPolicyValidator> validator =
        CreateValidator(std::move(policy_response));
    ValidatePolicy(check_action, std::move(validator));
  }

  void ValidatePolicy(
      testing::Action<void(UserCloudPolicyValidator*)> check_action,
      std::unique_ptr<UserCloudPolicyValidator> validator) {
    // Run validation and check the result.
    EXPECT_CALL(*this, ValidationCompletion(validator.get()))
        .WillOnce(check_action);

    validator->RunValidation();
    ValidationCompletion(validator.get());
    Mock::VerifyAndClearExpectations(this);
  }

  std::unique_ptr<UserCloudPolicyValidator> CreateValidator(
      std::unique_ptr<em::PolicyFetchResponse> policy_response) {
    std::string public_key = PolicyBuilder::GetPublicTestKeyAsString();
    EXPECT_FALSE(public_key.empty());

    const std::string& verification_data =
        policy_response->new_public_key_verification_data();
    const std::string& verification_data_signature =
        policy_response->new_public_key_verification_data_signature();

    auto validator = std::make_unique<UserCloudPolicyValidator>(
        std::move(policy_response),
        base::SingleThreadTaskRunner::GetCurrentDefault());
    validator->ValidateTimestamp(timestamp_, timestamp_option_);
    if (validate_by_gaia_id_) {
      validator->ValidateUsernameAndGaiaId(
          /*expected_user=*/std::string(), PolicyBuilder::kFakeGaiaId);
    } else {
      validator->ValidateUsername(PolicyBuilder::kFakeUsername);
    }
    if (!owning_domain_.empty())
      validator->ValidateDomain(owning_domain_);
    validator->ValidateDMToken(existing_dm_token_, dm_token_option_);
    validator->ValidateDeviceId(existing_device_id_, device_id_option_);
    validator->ValidatePolicyType(dm_protocol::kChromeUserPolicyType);
    validator->ValidatePayload();
    validator->ValidateCachedKey(public_key, cached_key_signature_,
                                 owning_domain_, verification_data,
                                 verification_data_signature);
    if (allow_key_rotation_) {
      validator->ValidateSignatureAllowingRotation(public_key, owning_domain_);
      validator->ValidateInitialKey(owning_domain_);
    } else {
      validator->ValidateSignature(public_key);
    }

    if (validate_values_) {
      validator->ValidateValues(
          std::make_unique<FakeUserPolicyValueValidator>());
    }

    return validator;
  }

  void CheckSuccessfulValidation(UserCloudPolicyValidator* validator) {
    EXPECT_TRUE(validator->success());
    EXPECT_EQ(policy_.policy().SerializeAsString(),
              validator->policy()->SerializeAsString());
    EXPECT_EQ(policy_.policy_data().SerializeAsString(),
              validator->policy_data()->SerializeAsString());
    EXPECT_EQ(policy_.payload().SerializeAsString(),
              validator->payload()->SerializeAsString());
  }

  void CheckValueValidation(UserCloudPolicyValidator* validator) {
    std::unique_ptr<CloudPolicyValidatorBase::ValidationResult>
        validation_result = validator->GetValidationResult();
    ASSERT_EQ(1u, validation_result->value_validation_issues.size());
    const ValueValidationIssue& result =
        validation_result->value_validation_issues[0];
    EXPECT_EQ(kPolicyName, result.policy_name);
    EXPECT_EQ(kSeverity, result.severity);
    EXPECT_EQ(kMessage, result.message);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::Time timestamp_;
  CloudPolicyValidatorBase::ValidateTimestampOption timestamp_option_;
  CloudPolicyValidatorBase::ValidateDMTokenOption dm_token_option_;
  CloudPolicyValidatorBase::ValidateDeviceIdOption device_id_option_;
  std::string signing_key_;
  bool allow_key_rotation_;
  std::string existing_dm_token_;
  std::string existing_device_id_;
  std::string owning_domain_;
  std::string cached_key_signature_;
  bool validate_by_gaia_id_;
  bool validate_values_;

  UserPolicyBuilder policy_;

 private:
  MOCK_METHOD1(ValidationCompletion, void(UserCloudPolicyValidator* validator));
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(CloudPolicyValidatorTest,
       SuccessfulValidationWithDisableKeyVerificationOnTestImage) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(switches::kDisablePolicyKeyVerification);
  const char kLsbRelease[] =
      "CHROMEOS_RELEASE_NAME=Chrome OS\n"
      "CHROMEOS_RELEASE_VERSION=1.2.3.4\n"
      "CHROMEOS_RELEASE_TRACK=testimage-channel\n";
  base::test::ScopedChromeOSVersionInfo version(kLsbRelease, base::Time());
  EXPECT_TRUE(base::SysInfo::IsRunningOnChromeOS());

  // Should not crash when creating a CloudPolicyValidator. Runs validation
  // successfully.
  Validate(Invoke(this, &CloudPolicyValidatorTest::CheckSuccessfulValidation));
}

TEST_F(CloudPolicyValidatorTest,
       CrashIfDisableKeyVerificationWithoutTestImage) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(switches::kDisablePolicyKeyVerification);
  const char kLsbRelease[] =
      "CHROMEOS_RELEASE_NAME=Chrome OS\n"
      "CHROMEOS_RELEASE_VERSION=1.2.3.4\n"
      "CHROMEOS_RELEASE_TRACK=stable-channel\n";
  base::test::ScopedChromeOSVersionInfo version(kLsbRelease, base::Time());
  EXPECT_TRUE(base::SysInfo::IsRunningOnChromeOS());

  // Should crash when creating a CloudPolicyValidator.
  EXPECT_DEATH_IF_SUPPORTED(
      {
        policy_.Build();
        std::unique_ptr<UserCloudPolicyValidator> validator =
            CreateValidator(policy_.GetCopy());
      },
      "");
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(CloudPolicyValidatorTest, SuccessfulValidation) {
  Validate(Invoke(this, &CloudPolicyValidatorTest::CheckSuccessfulValidation));
}

TEST_F(CloudPolicyValidatorTest, SuccessfulRunValidation) {
  policy_.Build();
  std::unique_ptr<UserCloudPolicyValidator> validator =
      CreateValidator(policy_.GetCopy());
  // Run validation immediately (no background tasks).
  validator->RunValidation();
  CheckSuccessfulValidation(validator.get());
}

TEST_F(CloudPolicyValidatorTest, SuccessfulRunValidationWithNoExistingDMToken) {
  existing_dm_token_.clear();
  Validate(Invoke(this, &CloudPolicyValidatorTest::CheckSuccessfulValidation));
}

TEST_F(CloudPolicyValidatorTest, SuccessfulRunValidationWithNoDMTokens) {
  existing_dm_token_.clear();
  policy_.policy_data().clear_request_token();
  dm_token_option_ = CloudPolicyValidatorBase::DM_TOKEN_NOT_REQUIRED;
  Validate(Invoke(this, &CloudPolicyValidatorTest::CheckSuccessfulValidation));
}

TEST_F(CloudPolicyValidatorTest,
       SuccessfulRunValidationWithNoExistingDeviceId) {
  existing_device_id_.clear();
  Validate(Invoke(this, &CloudPolicyValidatorTest::CheckSuccessfulValidation));
}

TEST_F(CloudPolicyValidatorTest, SuccessfulRunValidationWithNoDeviceId) {
  existing_device_id_.clear();
  policy_.policy_data().clear_device_id();
  device_id_option_ = CloudPolicyValidatorBase::DEVICE_ID_NOT_REQUIRED;
  Validate(Invoke(this, &CloudPolicyValidatorTest::CheckSuccessfulValidation));
}

TEST_F(CloudPolicyValidatorTest,
       SuccessfulRunValidationWithTimestampFromTheFuture) {
  base::Time timestamp(timestamp_ + base::Hours(3));
  policy_.policy_data().set_timestamp(
      (timestamp - base::Time::UnixEpoch()).InMilliseconds());
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_OK));
}

TEST_F(CloudPolicyValidatorTest, SuccessfulValidationWithSignatureTypeSHA1) {
  policy_.SetSignatureType(em::PolicyFetchRequest::SHA1_RSA);
  policy_.policy_data().set_policy_type(
      dm_protocol::kChromeMachineLevelUserCloudPolicyType);
  policy_.Build();
  std::unique_ptr<UserCloudPolicyValidator> validator =
      CreateValidator(policy_.GetCopy());
  validator->ValidatePolicyType(
      dm_protocol::kChromeMachineLevelUserCloudPolicyType);
  ValidatePolicy(CheckStatus(CloudPolicyValidatorBase::VALIDATION_OK),
                 std::move(validator));
}

// Assume that if a policy blob does not have `policy_data_signature_type` set,
// the blob is signed with SHA1_RSA.
TEST_F(CloudPolicyValidatorTest, SuccessfulValidationWithMissingSignatureType) {
  policy_.SetSignatureType(em::PolicyFetchRequest::SHA1_RSA);
  policy_.policy_data().set_policy_type(
      dm_protocol::kChromeMachineLevelUserCloudPolicyType);
  policy_.Build();
  policy_.policy().clear_policy_data_signature_type();
  std::unique_ptr<UserCloudPolicyValidator> validator =
      CreateValidator(policy_.GetCopy());
  validator->ValidatePolicyType(
      dm_protocol::kChromeMachineLevelUserCloudPolicyType);
  ValidatePolicy(CheckStatus(CloudPolicyValidatorBase::VALIDATION_OK),
                 std::move(validator));
}

TEST_F(CloudPolicyValidatorTest, SuccessfulValidationWithSignatureTypeSHA256) {
  policy_.SetSignatureType(em::PolicyFetchRequest::SHA256_RSA);
  policy_.policy_data().set_policy_type(
      dm_protocol::kChromeMachineLevelUserCloudPolicyType);
  policy_.Build();
  std::unique_ptr<UserCloudPolicyValidator> validator =
      CreateValidator(policy_.GetCopy());
  validator->ValidatePolicyType(
      dm_protocol::kChromeMachineLevelUserCloudPolicyType);
  ValidatePolicy(CheckStatus(CloudPolicyValidatorBase::VALIDATION_OK),
                 std::move(validator));
}

// Treat `em::PolicyFetchRequest::NONE` in `policy_data_signature_type`
// as unsigned, which is not supported.
TEST_F(CloudPolicyValidatorTest, FailedValidationWithSignatureTypeNONE) {
  policy_.SetSignatureType(em::PolicyFetchRequest::SHA1_RSA);
  policy_.policy_data().set_policy_type(dm_protocol::kChromeUserPolicyType);
  policy_.Build();
  policy_.policy().set_policy_data_signature_type(em::PolicyFetchRequest::NONE);
  std::unique_ptr<UserCloudPolicyValidator> validator =
      CreateValidator(policy_.GetCopy());
  validator->ValidatePolicyType(dm_protocol::kChromeUserPolicyType);
  ValidatePolicy(
      CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE),
      std::move(validator));
}

TEST_F(CloudPolicyValidatorTest, UsernameCanonicalization) {
  policy_.policy_data().set_username(
      base::ToUpperASCII(PolicyBuilder::kFakeUsername));
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_OK));
}

TEST_F(CloudPolicyValidatorTest, ErrorNoPolicyType) {
  policy_.policy_data().clear_policy_type();
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_WRONG_POLICY_TYPE));
}

TEST_F(CloudPolicyValidatorTest, ErrorWrongPolicyType) {
  policy_.policy_data().set_policy_type("invalid");
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_WRONG_POLICY_TYPE));
}

TEST_F(CloudPolicyValidatorTest, ErrorNoTimestamp) {
  policy_.policy_data().clear_timestamp();
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_TIMESTAMP));
}

TEST_F(CloudPolicyValidatorTest, IgnoreMissingTimestamp) {
  timestamp_option_ = CloudPolicyValidatorBase::TIMESTAMP_NOT_VALIDATED;
  policy_.policy_data().clear_timestamp();
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_OK));
}

TEST_F(CloudPolicyValidatorTest, ErrorOldTimestamp) {
  base::Time timestamp(timestamp_ - base::Minutes(5));
  policy_.policy_data().set_timestamp(timestamp.InMillisecondsSinceUnixEpoch());
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_TIMESTAMP));
}

TEST_F(CloudPolicyValidatorTest, ErrorNoDMToken) {
  policy_.policy_data().clear_request_token();
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_DM_TOKEN));
}

TEST_F(CloudPolicyValidatorTest, ErrorNoDMTokenNotRequired) {
  // Even though DM tokens are not required, if the existing policy has a token,
  // we should still generate an error if the new policy has none.
  policy_.policy_data().clear_request_token();
  dm_token_option_ = CloudPolicyValidatorBase::DM_TOKEN_NOT_REQUIRED;
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_DM_TOKEN));
}

TEST_F(CloudPolicyValidatorTest, ErrorNoDMTokenNoTokenPassed) {
  // Mimic the first fetch of policy (no existing DM token) - should still
  // complain about not having any DM token.
  existing_dm_token_.clear();
  policy_.policy_data().clear_request_token();
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_DM_TOKEN));
}

TEST_F(CloudPolicyValidatorTest, ErrorInvalidDMToken) {
  policy_.policy_data().set_request_token("invalid");
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_DM_TOKEN));
}

TEST_F(CloudPolicyValidatorTest, ErrorNoDeviceId) {
  policy_.policy_data().clear_device_id();
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_DEVICE_ID));
}

TEST_F(CloudPolicyValidatorTest, ErrorNoDeviceIdNotRequired) {
  // Even though device ids are not required, if the existing policy has a
  // device id, we should still generate an error if the new policy has none.
  policy_.policy_data().clear_device_id();
  device_id_option_ = CloudPolicyValidatorBase::DEVICE_ID_NOT_REQUIRED;
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_DEVICE_ID));
}

TEST_F(CloudPolicyValidatorTest, ErrorNoDeviceIdNoDeviceIdPassed) {
  // Mimic the first fetch of policy (no existing device id) - should still
  // complain about not having any device id.
  existing_device_id_.clear();
  policy_.policy_data().clear_device_id();
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_DEVICE_ID));
}

TEST_F(CloudPolicyValidatorTest, ErrorInvalidDeviceId) {
  policy_.policy_data().set_device_id("invalid");
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_DEVICE_ID));
}

TEST_F(CloudPolicyValidatorTest, ErrorNoPolicyValue) {
  policy_.clear_payload();
  Validate(
      CheckStatus(CloudPolicyValidatorBase::VALIDATION_POLICY_PARSE_ERROR));
}

TEST_F(CloudPolicyValidatorTest, ErrorInvalidPolicyValue) {
  policy_.clear_payload();
  policy_.policy_data().set_policy_value("invalid");
  Validate(
      CheckStatus(CloudPolicyValidatorBase::VALIDATION_POLICY_PARSE_ERROR));
}

TEST_F(CloudPolicyValidatorTest, ErrorNoUsername) {
  validate_by_gaia_id_ = false;
  policy_.policy_data().clear_username();
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_USER));
}

TEST_F(CloudPolicyValidatorTest, ErrorInvalidUsername) {
  validate_by_gaia_id_ = false;
  policy_.policy_data().set_username("invalid@example.com");
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_USER));
}

TEST_F(CloudPolicyValidatorTest, SuccessfulByUsername) {
  validate_by_gaia_id_ = false;
  policy_.policy_data().clear_gaia_id();
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_OK));
}

TEST_F(CloudPolicyValidatorTest, ErrorNoGaiaId) {
  policy_.policy_data().clear_gaia_id();
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_USER));
}

TEST_F(CloudPolicyValidatorTest, ErrorInvalidGaiaId) {
  policy_.policy_data().set_gaia_id("other-gaia-id");
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_USER));
}

TEST_F(CloudPolicyValidatorTest, ErrorErrorMessage) {
  policy_.policy().set_error_message("error");
  Validate(
      CheckStatus(CloudPolicyValidatorBase::VALIDATION_ERROR_CODE_PRESENT));
}

TEST_F(CloudPolicyValidatorTest, ErrorErrorCode) {
  policy_.policy().set_error_code(42);
  Validate(
      CheckStatus(CloudPolicyValidatorBase::VALIDATION_ERROR_CODE_PRESENT));
}

TEST_F(CloudPolicyValidatorTest, ErrorNoSignature) {
  policy_.UnsetSigningKey();
  policy_.UnsetNewSigningKey();
  policy_.policy().clear_policy_data_signature();
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE));
}

TEST_F(CloudPolicyValidatorTest, ErrorInvalidSignature) {
  policy_.UnsetSigningKey();
  policy_.UnsetNewSigningKey();
  policy_.policy().set_policy_data_signature("invalid");
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE));
}

TEST_F(CloudPolicyValidatorTest, ErrorNoPublicKey) {
  policy_.UnsetSigningKey();
  policy_.UnsetNewSigningKey();
  policy_.policy().clear_new_public_key();
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE));
}

TEST_F(CloudPolicyValidatorTest, ErrorInvalidPublicKey) {
  policy_.UnsetSigningKey();
  policy_.UnsetNewSigningKey();
  policy_.policy().set_new_public_key("invalid");
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE));
}

TEST_F(CloudPolicyValidatorTest, ErrorNoPublicKeySignature) {
  policy_.UnsetSigningKey();
  policy_.UnsetNewSigningKey();
  policy_.policy().clear_new_public_key_signature();
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE));
}

TEST_F(CloudPolicyValidatorTest, ErrorInvalidPublicKeySignature) {
  policy_.UnsetSigningKey();
  policy_.UnsetNewSigningKey();
  policy_.policy().set_new_public_key_signature("invalid");
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE));
}

TEST_F(CloudPolicyValidatorTest, ErrorInvalidPublicKeyVerificationSignature) {
  policy_.Build();
  policy_.policy().set_new_public_key_verification_signature_deprecated(
      "invalid");
  policy_.policy().set_new_public_key_verification_data_signature("invalid");
  ValidatePolicy(
      CheckStatus(
          CloudPolicyValidatorBase::VALIDATION_BAD_KEY_VERIFICATION_SIGNATURE),
      policy_.GetCopy());
}

TEST_F(CloudPolicyValidatorTest, GoodNewSignatureEmptyDeprecatedSignature) {
  policy_.Build();
  policy_.policy().set_new_public_key_verification_signature_deprecated("");
  ValidatePolicy(CheckStatus(CloudPolicyValidatorBase::VALIDATION_OK),
                 policy_.GetCopy());
}

TEST_F(CloudPolicyValidatorTest, ErrorDomainMismatchForKeyVerification) {
  policy_.Build();
  policy_.policy().set_new_public_key_verification_data("invalid");
  // Generate a non-matching owning_domain, which should cause a validation
  // failure.
  owning_domain_ = "invalid.com";
  ValidatePolicy(
      CheckStatus(
          CloudPolicyValidatorBase::VALIDATION_BAD_KEY_VERIFICATION_SIGNATURE),
      policy_.GetCopy());
}

TEST_F(CloudPolicyValidatorTest, ErrorDomainExtractedFromUsernameMismatch) {
  // Generate a non-matching username domain, which should cause a validation
  // failure when we try to verify the signing key with it.
  policy_.policy_data().set_username("wonky@invalid.com");
  policy_.Build();
  // Pass an empty domain to tell validator to extract the domain from the
  // policy's |username| field.
  owning_domain_ = "";
  ValidatePolicy(
      CheckStatus(
          CloudPolicyValidatorBase::VALIDATION_BAD_KEY_VERIFICATION_SIGNATURE),
      policy_.GetCopy());
}

TEST_F(CloudPolicyValidatorTest, ErrorNoCachedKeySignature) {
  // Generate an empty cached_key_signature_ and this should cause a validation
  // error when we try to verify the signing key with it.
  cached_key_signature_ = "";
  policy_.Build();
  policy_.policy().set_new_public_key_verification_data("invalid");
  ValidatePolicy(
      CheckStatus(
          CloudPolicyValidatorBase::VALIDATION_BAD_KEY_VERIFICATION_SIGNATURE),
      policy_.GetCopy());
}

TEST_F(CloudPolicyValidatorTest, ErrorInvalidCachedKeySignature) {
  // Generate a key signature for a different key (one that does not match
  // the signing key) and this should cause a validation error when we try to
  // verify the signing key with it.
  cached_key_signature_ = PolicyBuilder::GetTestOtherSigningKeySignature();
  policy_.Build();
  policy_.policy().set_new_public_key_verification_data("invalid");
  ValidatePolicy(
      CheckStatus(
          CloudPolicyValidatorBase::VALIDATION_BAD_KEY_VERIFICATION_SIGNATURE),
      policy_.GetCopy());
}

TEST_F(CloudPolicyValidatorTest, SuccessfulNoDomainValidation) {
  // Don't pass in a domain - this tells the validation code to instead
  // extract the domain from the username.
  owning_domain_ = "";
  Validate(Invoke(this, &CloudPolicyValidatorTest::CheckSuccessfulValidation));
}

TEST_F(CloudPolicyValidatorTest, SuccessWhenDeprecatedKeySignatureInvalid) {
  // The case when the deprecated key signature is missing. The validation
  // should pass based on new_public_key_verification_data
  policy_.Build();
  policy_.policy().set_new_public_key_verification_signature_deprecated(
      "invalid");
  ValidatePolicy(CheckStatus(CloudPolicyValidatorBase::VALIDATION_OK),
                 policy_.GetCopy());
}

// This test is expected to fail when the deprecated signature will be removed.
TEST_F(CloudPolicyValidatorTest, SuccessWhenNewKeySignatureInvalid) {
  // The case when the deprecated key signature is missing. The validation
  // should pass based on new_public_key_verification_data
  policy_.Build();
  policy_.policy().set_new_public_key_verification_data_signature("invalid");
  ValidatePolicy(CheckStatus(CloudPolicyValidatorBase::VALIDATION_OK),
                 policy_.GetCopy());
}

TEST_F(CloudPolicyValidatorTest, ErrorNoRotationAllowed) {
  allow_key_rotation_ = false;
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE));
}

TEST_F(CloudPolicyValidatorTest, NoRotation) {
  allow_key_rotation_ = false;
  policy_.UnsetNewSigningKey();
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_OK));
}

TEST_F(CloudPolicyValidatorTest, ValueValidation) {
  validate_values_ = true;
  Validate(Invoke(this, &CloudPolicyValidatorTest::CheckValueValidation));
}

}  // namespace

}  // namespace policy
