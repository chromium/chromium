// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/device_management/dm_message.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/time/time.h"
#include "chrome/enterprise_companion/device_management_storage/dm_storage.h"
#include "chrome/updater/device_management/dm_policy_builder_for_testing.h"
#include "chrome/updater/device_management/dm_response_validator.h"
#include "chrome/updater/protos/omaha_settings.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(DMMessage, GetRegisterBrowserRequestData) {
  std::string message = GetRegisterBrowserRequestData();
  EXPECT_FALSE(message.empty());
}

TEST(DMMessage, GetPolicyFetchRequestData) {
  std::string policy_type("google/machine-level-omaha");
  std::unique_ptr<
      ::wireless_android_enterprise_devicemanagement::OmahaSettingsClientProto>
      omaha_settings = GetDefaultTestingOmahaPolicyProto();
  std::unique_ptr<DMPolicyBuilderForTesting> policy_builder =
      DMPolicyBuilderForTesting::CreateInstanceWithOptions(
          true /* first request */, false /* rotate to new key */,
          DMPolicyBuilderForTesting::SigningOption::kSignNormally,
          "test-dm-token", "test-device-id");
  std::string policy_response_string(
      policy_builder->GetResponseBlobForPolicyPayload(
          policy_type, omaha_settings->SerializeAsString()));

  device_management_storage::CachedPolicyInfo policy_info;
  ASSERT_TRUE(policy_info.Populate(policy_response_string));
  const std::string request_data =
      GetPolicyFetchRequestData(policy_type, policy_info);
  EXPECT_FALSE(request_data.empty());

  enterprise_management::DeviceManagementRequest dm_request;
  ASSERT_TRUE(dm_request.ParseFromString(request_data));
  ASSERT_TRUE(dm_request.has_policy_request());
  const enterprise_management::DevicePolicyRequest& device_policy_request =
      dm_request.policy_request();
  ASSERT_TRUE(device_policy_request.has_reason());
  EXPECT_EQ(device_policy_request.reason(),
            enterprise_management::DevicePolicyRequest::SCHEDULED);
}

TEST(DMMessage, ParseDeviceRegistrationResponse) {
  enterprise_management::DeviceManagementResponse dm_response;
  dm_response.mutable_register_response()->set_device_management_token(
      "test-dm-token-foo");
  EXPECT_EQ(ParseDeviceRegistrationResponse(dm_response.SerializeAsString()),
            "test-dm-token-foo");
}

TEST(DMMessage, ParsePolicyFetchResponse) {
  const std::string policy_type = "google/machine-level-omaha";
  const base::Time test_start_time = base::Time::Now() - base::Milliseconds(1);

  // Test DM response with first policy fetch request.
  std::unique_ptr<::enterprise_management::DeviceManagementResponse>
      dm_response = GetDefaultTestingPolicyFetchDMResponse(
          true /* first_request */, false /* rotate_to_new_key */,
          DMPolicyBuilderForTesting::SigningOption::kSignNormally);

  device_management_storage::CachedPolicyInfo initial_policy_info;
  std::vector<PolicyValidationResult> validation_results;
  DMPolicyMap policy_map = ParsePolicyFetchResponse(
      dm_response->SerializeAsString(), initial_policy_info, "test-dm-token",
      "test-device-id", validation_results);
  EXPECT_TRUE(validation_results.empty());
  EXPECT_EQ(policy_map.size(), size_t{1});
  EXPECT_NE(policy_map.find(policy_type), policy_map.end());
  std::string policy_data = policy_map[policy_type];

  device_management_storage::CachedPolicyInfo updated_policy_info;
  updated_policy_info.Populate(policy_data);
  EXPECT_FALSE(updated_policy_info.public_key().empty());
  EXPECT_GE(base::Time::UnixEpoch() +
                base::Milliseconds(updated_policy_info.timestamp()),
            test_start_time);

  // Test the case when public key is not rotated.
  dm_response = GetDefaultTestingPolicyFetchDMResponse(
      false /* first_request */, false /* rotate_to_new_key */,
      DMPolicyBuilderForTesting::SigningOption::kSignNormally);
  policy_map = ParsePolicyFetchResponse(dm_response->SerializeAsString(),
                                        updated_policy_info, "test-dm-token",
                                        "test-device-id", validation_results);
  EXPECT_TRUE(validation_results.empty());
  EXPECT_EQ(policy_map.size(), size_t{1});
  EXPECT_NE(policy_map.find(policy_type), policy_map.end());

  device_management_storage::CachedPolicyInfo updated_policy_info2;
  updated_policy_info.Populate(policy_map[policy_type]);
  EXPECT_TRUE(updated_policy_info2.public_key().empty());

  // Test response that rotates to a new public key.
  dm_response = GetDefaultTestingPolicyFetchDMResponse(
      false /* first_request */, true /* rotate_to_new_key */,
      DMPolicyBuilderForTesting::SigningOption::kSignNormally);

  policy_map = ParsePolicyFetchResponse(dm_response->SerializeAsString(),
                                        updated_policy_info, "test-dm-token",
                                        "test-device-id", validation_results);
  EXPECT_TRUE(validation_results.empty());
  EXPECT_EQ(policy_map.size(), size_t{1});
  EXPECT_NE(policy_map.find(policy_type), policy_map.end());

  // Verify that we got a new public key.
  device_management_storage::CachedPolicyInfo updated_policy_info3;
  updated_policy_info3.Populate(policy_map[policy_type]);
  std::string new_public_key = updated_policy_info3.public_key();
  EXPECT_FALSE(new_public_key.empty());
  EXPECT_NE(new_public_key, updated_policy_info.public_key());
  EXPECT_GE(updated_policy_info3.timestamp(), updated_policy_info.timestamp());
}

TEST(DMMessage, ResponseValidation) {
  const std::string policy_type = "google/machine-level-omaha";

  // 1) Clients rejects DM responses if it is not intended for the
  // expected device ID or DM token.
  std::unique_ptr<::enterprise_management::DeviceManagementResponse>
      dm_response = GetDefaultTestingPolicyFetchDMResponse(
          true /* first_request */, false /* rotate_to_new_key */,
          DMPolicyBuilderForTesting::SigningOption::kSignNormally);
  const std::string dm_response_data = dm_response->SerializeAsString();

  device_management_storage::CachedPolicyInfo initial_policy_info;
  const std::string bad_dm_token = "bad-dm-token";
  std::vector<PolicyValidationResult> validation_results;
  DMPolicyMap policy_map = ParsePolicyFetchResponse(
      dm_response_data, initial_policy_info, bad_dm_token, "test-device-id",
      validation_results);
  EXPECT_EQ(validation_results.size(), size_t{1});
  EXPECT_EQ(validation_results[0].policy_type, policy_type);
  EXPECT_EQ(validation_results[0].status,
            PolicyValidationResult::Status::kValidationBadDMToken);
  EXPECT_TRUE(validation_results[0].issues.empty());
  EXPECT_TRUE(policy_map.empty());
  validation_results.clear();

  const std::string bad_devide_id = "unexpected-device-id";
  policy_map = ParsePolicyFetchResponse(dm_response_data, initial_policy_info,
                                        "test-dm-token", bad_devide_id,
                                        validation_results);
  EXPECT_EQ(validation_results.size(), size_t{1});
  EXPECT_EQ(validation_results[0].policy_type, policy_type);
  EXPECT_EQ(validation_results[0].status,
            PolicyValidationResult::Status::kValidationBadDeviceID);
  EXPECT_TRUE(validation_results[0].issues.empty());
  EXPECT_TRUE(policy_map.empty());
  validation_results.clear();

  // 2) Client must have a cached public key other than the first request.
  dm_response = GetDefaultTestingPolicyFetchDMResponse(
      false /* first_request */, false /* rotate_to_new_key */,
      DMPolicyBuilderForTesting::SigningOption::kSignNormally);
  policy_map = ParsePolicyFetchResponse(dm_response->SerializeAsString(),
                                        initial_policy_info, "test-dm-token",
                                        "test-device-id", validation_results);
  EXPECT_EQ(validation_results.size(), size_t{1});
  EXPECT_EQ(validation_results[0].policy_type, policy_type);
  EXPECT_EQ(validation_results[0].status,
            PolicyValidationResult::Status::kValidationBadSignature);
  EXPECT_TRUE(validation_results[0].issues.empty());
  EXPECT_TRUE(policy_map.empty());
  validation_results.clear();

  // 3) Client should reject response if the public key is not signed properly.

  // First create a DM response to update cached policy info (gets a key
  // to sign the new key).
  dm_response = GetDefaultTestingPolicyFetchDMResponse(
      true /* first_request */, false /* rotate_to_new_key */,
      DMPolicyBuilderForTesting::SigningOption::kSignNormally);
  policy_map = ParsePolicyFetchResponse(dm_response->SerializeAsString(),
                                        initial_policy_info, "test-dm-token",
                                        "test-device-id", validation_results);
  EXPECT_TRUE(validation_results.empty());
  device_management_storage::CachedPolicyInfo updated_policy_info;
  updated_policy_info.Populate(policy_map[policy_type]);
  EXPECT_FALSE(updated_policy_info.public_key().empty());

  dm_response = GetDefaultTestingPolicyFetchDMResponse(
      true /* first_request */, false /* rotate_to_new_key */,
      DMPolicyBuilderForTesting::SigningOption::kTamperKeySignature);
  policy_map = ParsePolicyFetchResponse(dm_response->SerializeAsString(),
                                        updated_policy_info, "test-dm-token",
                                        "test-device-id", validation_results);
  EXPECT_EQ(validation_results.size(), size_t{1});
  EXPECT_EQ(validation_results[0].policy_type, policy_type);
  EXPECT_EQ(
      validation_results[0].status,
      PolicyValidationResult::Status::kValidationBadKeyVerificationSignature);
  EXPECT_TRUE(validation_results[0].issues.empty());
  EXPECT_TRUE(policy_map.empty());
  validation_results.clear();

  // 4) Client should reject response if policy data is not signed properly.
  dm_response = GetDefaultTestingPolicyFetchDMResponse(
      true /* first_request */, false /* rotate_to_new_key */,
      DMPolicyBuilderForTesting::SigningOption::kTamperDataSignature);
  policy_map = ParsePolicyFetchResponse(dm_response->SerializeAsString(),
                                        initial_policy_info, "test-dm-token",
                                        "test-device-id", validation_results);
  EXPECT_EQ(validation_results.size(), size_t{1});
  EXPECT_EQ(validation_results[0].policy_type, policy_type);
  EXPECT_EQ(validation_results[0].status,
            PolicyValidationResult::Status::kValidationBadSignature);
  EXPECT_TRUE(validation_results[0].issues.empty());
  EXPECT_TRUE(policy_map.empty());
  validation_results.clear();
}

}  // namespace updater
