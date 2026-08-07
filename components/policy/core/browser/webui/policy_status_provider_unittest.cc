// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/webui/policy_status_provider.h"

#include <memory>
#include <string>
#include <utility>

#include "base/values.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/resources/webui/mojom/policy.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {
namespace {

constexpr char kTestClientId[] = "test-client-id";
constexpr char kTestUsername[] = "user@example.com";
constexpr char kTestAssetId[] = "asset-123";
constexpr char kTestLocation[] = "location-xyz";
constexpr char kTestDirectoryApiId[] = "dir-api-456";
constexpr char kTestGaiaId[] = "gaia-789";
constexpr char kTestStatus[] = "OK";

class MockPolicyStatusObserver : public PolicyStatusProvider::Observer {
 public:
  MOCK_METHOD(void, OnPolicyStatusChanged, (), (override));
};

class TestPolicyStatusProvider : public PolicyStatusProvider {
 public:
  TestPolicyStatusProvider() = default;
  ~TestPolicyStatusProvider() override = default;

  void TriggerNotifyStatusChange() { NotifyStatusChange(); }

  using PolicyStatusProvider::DictStatusToMojo;
};

TEST(PolicyStatusProviderTest, DefaultStatus) {
  PolicyStatusProvider provider;
  EXPECT_TRUE(provider.GetStatus().empty());

  policy::mojom::StatusPtr status_mojo = provider.GetStatusMojo();
  ASSERT_FALSE(status_mojo.is_null());
  EXPECT_TRUE(status_mojo->client_id.empty());
}

TEST(PolicyStatusProviderTest, ObserverNotification) {
  TestPolicyStatusProvider provider;
  testing::StrictMock<MockPolicyStatusObserver> observer;

  provider.AddObserver(&observer);
  EXPECT_CALL(observer, OnPolicyStatusChanged()).Times(1);
  provider.TriggerNotifyStatusChange();
  testing::Mock::VerifyAndClearExpectations(&observer);

  provider.RemoveObserver(&observer);
  EXPECT_CALL(observer, OnPolicyStatusChanged()).Times(0);
  provider.TriggerNotifyStatusChange();
}

TEST(PolicyStatusProviderTest, GetStatusFromPolicyDataNull) {
  base::DictValue status =
      PolicyStatusProvider::GetStatusFromPolicyData(nullptr);
  const std::string* client_id = status.FindString(kClientIdKey);
  ASSERT_TRUE(client_id);
  EXPECT_EQ(*client_id, std::string());

  const std::string* username = status.FindString(kUsernameKey);
  ASSERT_TRUE(username);
  EXPECT_EQ(*username, std::string());
}

TEST(PolicyStatusProviderTest, GetStatusFromPolicyDataPopulated) {
  enterprise_management::PolicyData policy_data;
  policy_data.set_device_id(kTestClientId);
  policy_data.set_username(kTestUsername);
  policy_data.set_annotated_asset_id(kTestAssetId);
  policy_data.set_annotated_location(kTestLocation);
  policy_data.set_directory_api_id(kTestDirectoryApiId);
  policy_data.set_gaia_id(kTestGaiaId);

  base::DictValue status =
      PolicyStatusProvider::GetStatusFromPolicyData(&policy_data);

  EXPECT_EQ(*status.FindString(kClientIdKey), kTestClientId);
  EXPECT_EQ(*status.FindString(kUsernameKey), kTestUsername);
  EXPECT_EQ(*status.FindString(kAssetIdKey), kTestAssetId);
  EXPECT_EQ(*status.FindString(kLocationKey), kTestLocation);
  EXPECT_EQ(*status.FindString(kDirectoryApiIdKey), kTestDirectoryApiId);
  EXPECT_EQ(*status.FindString(kGaiaIdKey), kTestGaiaId);
}

TEST(PolicyStatusProviderTest, PopulateStatusFromPolicyDataNull) {
  auto status = policy::mojom::Status::New();
  PolicyStatusProvider::PopulateStatusFromPolicyData(nullptr, status);
  EXPECT_EQ(status->client_id, std::string());
  EXPECT_EQ(status->username, std::string());
}

TEST(PolicyStatusProviderTest, PopulateStatusFromPolicyDataPopulated) {
  enterprise_management::PolicyData policy_data;
  policy_data.set_device_id(kTestClientId);
  policy_data.set_username(kTestUsername);
  policy_data.set_annotated_asset_id(kTestAssetId);
  policy_data.set_annotated_location(kTestLocation);
  policy_data.set_directory_api_id(kTestDirectoryApiId);
  policy_data.set_gaia_id(kTestGaiaId);

  auto status = policy::mojom::Status::New();
  PolicyStatusProvider::PopulateStatusFromPolicyData(&policy_data, status);

  EXPECT_EQ(status->client_id, kTestClientId);
  EXPECT_EQ(status->username, kTestUsername);
  EXPECT_EQ(status->asset_id, kTestAssetId);
  EXPECT_EQ(status->location, kTestLocation);
  EXPECT_EQ(status->directory_api_id, kTestDirectoryApiId);
  EXPECT_EQ(status->gaia_id, kTestGaiaId);
}

TEST(PolicyStatusProviderTest, DictStatusToMojo) {
  base::DictValue dict;
  dict.Set(kClientIdKey, kTestClientId);
  dict.Set("status", kTestStatus);
  dict.Set("error", false);
  dict.Set("policiesPushAvailable", true);

  policy::mojom::StatusPtr status =
      TestPolicyStatusProvider::DictStatusToMojo(dict);

  EXPECT_EQ(status->client_id, kTestClientId);
  EXPECT_EQ(status->status, kTestStatus);
  EXPECT_FALSE(status->error);
  EXPECT_TRUE(status->policies_push_available);
}

}  // namespace
}  // namespace policy
