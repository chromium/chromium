// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/device_management/dm_response_validator.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "chrome/enterprise_companion/device_management_storage/dm_storage.h"
#include "chrome/updater/device_management/dm_message.h"
#include "chrome/updater/device_management/dm_policy_builder_for_testing.h"
#include "chrome/updater/protos/omaha_settings.pb.h"
#include "chrome/updater/test/unit_test_util.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

namespace edm = ::wireless_android_enterprise_devicemanagement;

class DMResponseValidatorTests : public ::testing::Test {
 protected:
  void GetCachedInfoWithPublicKey(
      device_management_storage::CachedPolicyInfo& cached_info) const;

  std::unique_ptr<::enterprise_management::DeviceManagementResponse>
  GetDMResponseWithOmahaPolicy(
      const edm::OmahaSettingsClientProto& omaha_settings) const;
};

void DMResponseValidatorTests::GetCachedInfoWithPublicKey(
    device_management_storage::CachedPolicyInfo& cached_info) const {
  std::unique_ptr<::enterprise_management::DeviceManagementResponse>
      dm_response = GetDefaultTestingPolicyFetchDMResponse(
          /*first_request=*/true, /*rotate_to_new_key=*/false,
          DMPolicyBuilderForTesting::SigningOption::kSignNormally);

  EXPECT_EQ(dm_response->policy_response().responses_size(), 1);
  const ::enterprise_management::PolicyFetchResponse& response =
      dm_response->policy_response().responses(0);
  std::string policy_fetch_response;
  EXPECT_TRUE(response.SerializeToString(&policy_fetch_response));
  cached_info.Populate(policy_fetch_response);
  EXPECT_FALSE(cached_info.public_key().empty());
}

std::unique_ptr<::enterprise_management::DeviceManagementResponse>
DMResponseValidatorTests::GetDMResponseWithOmahaPolicy(
    const edm::OmahaSettingsClientProto& omaha_settings) const {
  std::unique_ptr<DMPolicyBuilderForTesting> policy_builder =
      DMPolicyBuilderForTesting::CreateInstanceWithOptions(
          /*first_request=*/true, /*rotate_to_new_key=*/true,
          DMPolicyBuilderForTesting::SigningOption::kSignNormally,
          "test-dm-token", "test-device-id");
  DMPolicyMap policy_map;
  policy_map.emplace(kGoogleUpdatePolicyType,
                     omaha_settings.SerializeAsString());
  return policy_builder->BuildDMResponseForPolicies(policy_map);
}

TEST_F(DMResponseValidatorTests, ValidationOKWithoutPublicKey) {
  std::unique_ptr<::enterprise_management::DeviceManagementResponse>
      dm_response = GetDefaultTestingPolicyFetchDMResponse(
          /*first_request=*/true, /*rotate_to_new_key=*/false,
          DMPolicyBuilderForTesting::SigningOption::kSignNormally);

  EXPECT_EQ(dm_response->policy_response().responses_size(), 1);
  const ::enterprise_management::PolicyFetchResponse& response =
      dm_response->policy_response().responses(0);

  DMResponseValidator validator(device_management_storage::CachedPolicyInfo(),
                                "test-dm-token", "test-device-id");
  PolicyValidationResult validation_result;
  EXPECT_TRUE(validator.ValidatePolicyResponse(response, validation_result));
  EXPECT_EQ(validation_result.status,
            PolicyValidationResult::Status::kValidationOK);
  EXPECT_TRUE(validation_result.issues.empty());
}

TEST_F(DMResponseValidatorTests, ValidationOKWithPublicKey) {
  // Cached info should be created before parsing next policy response.
  device_management_storage::CachedPolicyInfo cached_info;
  GetCachedInfoWithPublicKey(cached_info);

  std::unique_ptr<::enterprise_management::DeviceManagementResponse>
      dm_response = GetDefaultTestingPolicyFetchDMResponse(
          /*first_request=*/false, /*rotate_to_new_key=*/false,
          DMPolicyBuilderForTesting::SigningOption::kSignNormally);

  EXPECT_EQ(dm_response->policy_response().responses_size(), 1);
  const ::enterprise_management::PolicyFetchResponse& response =
      dm_response->policy_response().responses(0);

  DMResponseValidator validator(cached_info, "test-dm-token", "test-device-id");
  PolicyValidationResult validation_result;
  EXPECT_TRUE(validator.ValidatePolicyResponse(response, validation_result));
  EXPECT_EQ(validation_result.status,
            PolicyValidationResult::Status::kValidationOK);
  EXPECT_TRUE(validation_result.issues.empty());
}

TEST_F(DMResponseValidatorTests, UnexpectedDMToken) {
  std::unique_ptr<::enterprise_management::DeviceManagementResponse>
      dm_response = GetDefaultTestingPolicyFetchDMResponse(
          /*first_request=*/true, /*rotate_to_new_key=*/false,
          DMPolicyBuilderForTesting::SigningOption::kSignNormally);

  EXPECT_EQ(dm_response->policy_response().responses_size(), 1);
  const ::enterprise_management::PolicyFetchResponse& response =
      dm_response->policy_response().responses(0);

  DMResponseValidator validator(device_management_storage::CachedPolicyInfo(),
                                "wrong-dm-token", "test-device-id");
  PolicyValidationResult validation_result;
  EXPECT_FALSE(validator.ValidatePolicyResponse(response, validation_result));
  EXPECT_EQ(validation_result.policy_type, "google/machine-level-omaha");
  EXPECT_EQ(validation_result.status,
            PolicyValidationResult::Status::kValidationBadDMToken);
  EXPECT_TRUE(validation_result.issues.empty());
}

TEST_F(DMResponseValidatorTests, UnexpectedDeviceID) {
  std::unique_ptr<::enterprise_management::DeviceManagementResponse>
      dm_response = GetDefaultTestingPolicyFetchDMResponse(
          /*first_request=*/true, /*rotate_to_new_key=*/false,
          DMPolicyBuilderForTesting::SigningOption::kSignNormally);

  EXPECT_EQ(dm_response->policy_response().responses_size(), 1);
  const ::enterprise_management::PolicyFetchResponse& response =
      dm_response->policy_response().responses(0);

  DMResponseValidator validator(device_management_storage::CachedPolicyInfo(),
                                "test-dm-token", "unexpected-device-id");
  PolicyValidationResult validation_result;
  EXPECT_FALSE(validator.ValidatePolicyResponse(response, validation_result));
  EXPECT_EQ(validation_result.policy_type, "google/machine-level-omaha");
  EXPECT_EQ(validation_result.status,
            PolicyValidationResult::Status::kValidationBadDeviceID);
  EXPECT_TRUE(validation_result.issues.empty());
}

TEST_F(DMResponseValidatorTests, NoCachedPublicKey) {
  // Verify that client must have a cached public key other than the first
  // request.
  std::unique_ptr<::enterprise_management::DeviceManagementResponse>
      dm_response = GetDefaultTestingPolicyFetchDMResponse(
          /*first_request=*/false, /*rotate_to_new_key=*/false,
          DMPolicyBuilderForTesting::SigningOption::kSignNormally);

  EXPECT_EQ(dm_response->policy_response().responses_size(), 1);
  const ::enterprise_management::PolicyFetchResponse& response =
      dm_response->policy_response().responses(0);

  DMResponseValidator validator(device_management_storage::CachedPolicyInfo(),
                                "test-dm-token", "test-device-id");
  PolicyValidationResult validation_result;
  EXPECT_FALSE(validator.ValidatePolicyResponse(response, validation_result));
  EXPECT_EQ(validation_result.policy_type, "google/machine-level-omaha");
  EXPECT_EQ(validation_result.status,
            PolicyValidationResult::Status::kValidationBadSignature);
  EXPECT_TRUE(validation_result.issues.empty());
}

TEST_F(DMResponseValidatorTests, BadSignedPublicKey) {
  // Cached info should be created before parsing next policy response.
  device_management_storage::CachedPolicyInfo cached_info;
  GetCachedInfoWithPublicKey(cached_info);

  // Validation should fail if the public key is not signed properly.
  std::unique_ptr<::enterprise_management::DeviceManagementResponse>
      dm_response = GetDefaultTestingPolicyFetchDMResponse(
          /*first_request=*/true, /*rotate_to_new_key=*/false,
          DMPolicyBuilderForTesting::SigningOption::kSignNormally);

  EXPECT_EQ(dm_response->policy_response().responses_size(), 1);
  const ::enterprise_management::PolicyFetchResponse& response =
      dm_response->policy_response().responses(0);

  DMResponseValidator validator(cached_info, "test-dm-token", "test-device-id");
  PolicyValidationResult validation_result;
  EXPECT_FALSE(validator.ValidatePolicyResponse(response, validation_result));
  EXPECT_EQ(validation_result.policy_type, "google/machine-level-omaha");
  EXPECT_EQ(
      validation_result.status,
      PolicyValidationResult::Status::kValidationBadKeyVerificationSignature);
  EXPECT_TRUE(validation_result.issues.empty());
}

TEST_F(DMResponseValidatorTests, BadSignedPolicyData) {
  // Validation should fail if policy data is not signed properly.
  std::unique_ptr<::enterprise_management::DeviceManagementResponse>
      dm_response = GetDefaultTestingPolicyFetchDMResponse(
          /*first_request=*/true, /*rotate_to_new_key=*/false,
          DMPolicyBuilderForTesting::SigningOption::kTamperDataSignature);

  EXPECT_EQ(dm_response->policy_response().responses_size(), 1);
  const ::enterprise_management::PolicyFetchResponse& response =
      dm_response->policy_response().responses(0);

  DMResponseValidator validator(device_management_storage::CachedPolicyInfo(),
                                "test-dm-token", "test-device-id");
  PolicyValidationResult validation_result;
  EXPECT_FALSE(validator.ValidatePolicyResponse(response, validation_result));
  EXPECT_EQ(validation_result.status,
            PolicyValidationResult::Status::kValidationBadSignature);
  EXPECT_TRUE(validation_result.issues.empty());
}

TEST_F(DMResponseValidatorTests, OmahaPolicyWithBadValues) {
  edm::OmahaSettingsClientProto omaha_settings;

  omaha_settings.set_auto_update_check_period_minutes(43201);
  omaha_settings.set_download_preference("InvalidDownloadPreference");
  omaha_settings.mutable_updates_suppressed()->set_start_hour(25);
  omaha_settings.mutable_updates_suppressed()->set_start_minute(-1);
  omaha_settings.mutable_updates_suppressed()->set_duration_min(1000);
  omaha_settings.set_proxy_mode("weird_proxy_mode");
  omaha_settings.set_proxy_server("unexpected_proxy");
  omaha_settings.set_proxy_pac_url("foo.c/proxy.pa");
  omaha_settings.set_install_default(edm::INSTALL_DEFAULT_DISABLED);
  omaha_settings.set_update_default(edm::MANUAL_UPDATES_ONLY);

  edm::ApplicationSettings app;
  app.set_app_guid(test::kChromeAppId);

  app.set_install(edm::INSTALL_DISABLED);
  app.set_update(edm::AUTOMATIC_UPDATES_ONLY);
  app.set_target_channel("");
  app.set_target_version_prefix("");
  app.set_rollback_to_target_version(edm::ROLLBACK_TO_TARGET_VERSION_DISABLED);
  omaha_settings.mutable_application_settings()->Add(std::move(app));

  std::unique_ptr<::enterprise_management::DeviceManagementResponse>
      dm_response = GetDMResponseWithOmahaPolicy(omaha_settings);

  EXPECT_EQ(dm_response->policy_response().responses_size(), 1);
  const ::enterprise_management::PolicyFetchResponse& response =
      dm_response->policy_response().responses(0);

  DMResponseValidator validator(device_management_storage::CachedPolicyInfo(),
                                "test-dm-token", "test-device-id");
  PolicyValidationResult validation_result;
  EXPECT_FALSE(validator.ValidatePolicyResponse(response, validation_result));
  EXPECT_EQ(validation_result.policy_type, kGoogleUpdatePolicyType);
  EXPECT_EQ(validation_result.status,
            PolicyValidationResult::Status::kValidationOK);
  EXPECT_EQ(validation_result.issues.size(), size_t{10});
  EXPECT_EQ(validation_result.issues[0].policy_name,
            "auto_update_check_period_minutes");
  EXPECT_EQ(validation_result.issues[0].severity,
            PolicyValueValidationIssue::Severity::kError);
  EXPECT_EQ(validation_result.issues[0].message,
            "Value out of range (0 - 43200): 43201");
  EXPECT_EQ(validation_result.issues[1].policy_name, "download_preference");
  EXPECT_EQ(validation_result.issues[1].severity,
            PolicyValueValidationIssue::Severity::kWarning);
  EXPECT_EQ(validation_result.issues[1].message,
            "Unrecognized download preference: InvalidDownloadPreference");
  EXPECT_EQ(validation_result.issues[2].policy_name,
            "updates_suppressed.start_hour");
  EXPECT_EQ(validation_result.issues[2].severity,
            PolicyValueValidationIssue::Severity::kError);
  EXPECT_EQ(validation_result.issues[2].message,
            "Value out of range(0 - 23): 25");
  EXPECT_EQ(validation_result.issues[3].policy_name,
            "updates_suppressed.start_minute");
  EXPECT_EQ(validation_result.issues[3].severity,
            PolicyValueValidationIssue::Severity::kError);
  EXPECT_EQ(validation_result.issues[3].message,
            "Value out of range(0 - 59): -1");
  EXPECT_EQ(validation_result.issues[4].policy_name,
            "updates_suppressed.duration_min");
  EXPECT_EQ(validation_result.issues[4].severity,
            PolicyValueValidationIssue::Severity::kError);
  EXPECT_EQ(validation_result.issues[4].message,
            "Value out of range(0 - 960): 1000");
  EXPECT_EQ(validation_result.issues[5].policy_name, "proxy_mode");
  EXPECT_EQ(validation_result.issues[5].severity,
            PolicyValueValidationIssue::Severity::kWarning);
  EXPECT_EQ(validation_result.issues[5].message,
            "Unrecognized proxy mode: weird_proxy_mode");
  EXPECT_EQ(validation_result.issues[6].policy_name, "proxy_server");
  EXPECT_EQ(validation_result.issues[6].severity,
            PolicyValueValidationIssue::Severity::kWarning);
  EXPECT_EQ(validation_result.issues[6].message,
            "Proxy server setting [unexpected_proxy] is ignored because proxy "
            "mode is not [fixed_servers]");
  EXPECT_EQ(validation_result.issues[7].policy_name, "proxy_pac_url");
  EXPECT_EQ(validation_result.issues[7].severity,
            PolicyValueValidationIssue::Severity::kWarning);
  EXPECT_EQ(validation_result.issues[7].message,
            "Proxy PAC URL setting [foo.c/proxy.pa] is ignored because proxy "
            "mode is not [pac_script]");
  EXPECT_EQ(validation_result.issues[8].policy_name, "target_channel");
  EXPECT_EQ(validation_result.issues[8].severity,
            PolicyValueValidationIssue::Severity::kWarning);
  EXPECT_EQ(validation_result.issues[8].message,
            "{8A69D345-D564-463C-AFF1-A69D9E530F96} empty policy value");
  EXPECT_EQ(validation_result.issues[9].policy_name, "target_version_prefix");
  EXPECT_EQ(validation_result.issues[9].severity,
            PolicyValueValidationIssue::Severity::kWarning);
  EXPECT_EQ(validation_result.issues[9].message,
            "{8A69D345-D564-463C-AFF1-A69D9E530F96} empty policy value");
}

}  // namespace updater
