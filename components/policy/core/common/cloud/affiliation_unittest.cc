// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/affiliation.h"

#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/policy/core/common/cloud/mock_user_cloud_policy_store.h"
#endif

using testing::IsEmpty;
using testing::UnorderedElementsAre;

namespace policy {

namespace {

constexpr char kAffiliationId1[] = "abc";
constexpr char kAffiliationId2[] = "def";

#if !BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kNonEmptyDmToken[] = "test-dm-token";

policy::MockCloudPolicyClient* ConnectNewMockClient(
    policy::CloudPolicyCore* core) {
  auto client = std::make_unique<policy::MockCloudPolicyClient>();
  auto* client_ptr = client.get();
  core->Connect(std::move(client));
  return client_ptr;
}
#endif

}  // namespace

TEST(CloudManagementAffiliationTest, Affiliated) {
  base::flat_set<std::string> user_ids;
  user_ids.insert(kAffiliationId1);
  user_ids.insert(kAffiliationId2);

  base::flat_set<std::string> device_ids;
  device_ids.insert(kAffiliationId1);

  EXPECT_TRUE(policy::IsAffiliated(user_ids, device_ids));
}

TEST(CloudManagementAffiliationTest, Unaffiliated) {
  base::flat_set<std::string> user_ids;
  user_ids.insert(kAffiliationId1);

  base::flat_set<std::string> device_ids;
  user_ids.insert(kAffiliationId2);

  EXPECT_FALSE(IsAffiliated(user_ids, device_ids));
}

TEST(CloudManagementAffiliationTest, UserIdsEmpty) {
  base::flat_set<std::string> user_ids;
  base::flat_set<std::string> device_ids;
  user_ids.insert(kAffiliationId1);

  EXPECT_FALSE(IsAffiliated(user_ids, device_ids));
}

TEST(CloudManagementAffiliationTest, DeviceIdsEmpty) {
  base::flat_set<std::string> user_ids;
  user_ids.insert(kAffiliationId1);
  base::flat_set<std::string> device_ids;

  EXPECT_FALSE(IsAffiliated(user_ids, device_ids));
}

TEST(CloudManagementAffiliationTest, BothIdsEmpty) {
  base::flat_set<std::string> user_ids;
  base::flat_set<std::string> device_ids;

  EXPECT_FALSE(IsAffiliated(user_ids, device_ids));
}

TEST(CloudManagementAffiliationTest, UserAffiliated) {
  base::flat_set<std::string> user_ids;
  base::flat_set<std::string> device_ids;

  // Empty affiliation IDs.
  EXPECT_FALSE(IsUserAffiliated(user_ids, device_ids, "user@managed.com"));

  user_ids.insert("aaaa");  // Only user affiliation IDs present.
  EXPECT_FALSE(IsUserAffiliated(user_ids, device_ids, "user@managed.com"));

  device_ids.insert("bbbb");  // Device and user IDs do not overlap.
  EXPECT_FALSE(IsUserAffiliated(user_ids, device_ids, "user@managed.com"));

  user_ids.insert("cccc");  // Device and user IDs do overlap.
  device_ids.insert("cccc");
  EXPECT_TRUE(IsUserAffiliated(user_ids, device_ids, "user@managed.com"));

  // Invalid email overrides match of affiliation IDs.
  EXPECT_FALSE(IsUserAffiliated(user_ids, device_ids, ""));
  EXPECT_FALSE(IsUserAffiliated(user_ids, device_ids, "user"));
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Tests getting affiliation IDs from core for user.
TEST(CloudManagementAffiliationTest, GetUserAffiliationIdsFromCore_User) {
  base::test::TaskEnvironment task_environment;

  policy::MockUserCloudPolicyStore store;
  policy::CloudPolicyCore core(
      policy::dm_protocol::kChromeUserPolicyType, std::string(), &store,
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      network::TestNetworkConnectionTracker::CreateGetter());

  // Connect and register client.
  policy::MockCloudPolicyClient* client = ConnectNewMockClient(&core);
  client->SetDMToken(kNonEmptyDmToken);

  // Set user affiliations ids in user level store.
  auto policy_data = std::make_unique<enterprise_management::PolicyData>();
  policy_data->add_user_affiliation_ids(kAffiliationId1);
  store.set_policy_data_for_testing(std::move(policy_data));

  auto returned_ids = GetAffiliationIdsFromCore(core, /*for_device=*/false);

  EXPECT_THAT(returned_ids, UnorderedElementsAre(kAffiliationId1));
}

// Tests getting affiliation IDs from core for device.
TEST(CloudManagementAffiliationTest, GetUserAffiliationIdsFromCore_Device) {
  base::test::TaskEnvironment task_environment;

  policy::MockUserCloudPolicyStore store;
  policy::CloudPolicyCore core(
      policy::dm_protocol::kChromeUserPolicyType, std::string(), &store,
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      network::TestNetworkConnectionTracker::CreateGetter());

  // Connect and register client.
  policy::MockCloudPolicyClient* client = ConnectNewMockClient(&core);
  client->SetDMToken(kNonEmptyDmToken);

  // Set user affiliations ids in user level store.
  auto policy_data = std::make_unique<enterprise_management::PolicyData>();
  policy_data->add_device_affiliation_ids(kAffiliationId1);
  store.set_policy_data_for_testing(std::move(policy_data));

  auto returned_ids = GetAffiliationIdsFromCore(core, /*for_device=*/true);

  EXPECT_THAT(returned_ids, UnorderedElementsAre(kAffiliationId1));
}

// Tests getting affiliations IDs from core when no client.
TEST(CloudManagementAffiliationTest, GetUserAffiliationIdsFromCore_NoClient) {
  base::test::TaskEnvironment task_environment;

  policy::MockUserCloudPolicyStore store;
  policy::CloudPolicyCore core(
      policy::dm_protocol::kChromeUserPolicyType, std::string(), &store,
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      network::TestNetworkConnectionTracker::CreateGetter());

  // Set user affiliations ids in user level store.
  auto policy_data = std::make_unique<enterprise_management::PolicyData>();
  policy_data->add_user_affiliation_ids(kAffiliationId1);
  store.set_policy_data_for_testing(std::move(policy_data));

  auto returned_ids = GetAffiliationIdsFromCore(core, /*for_device=*/false);

  EXPECT_THAT(returned_ids, IsEmpty());
}

// Tests getting affiliations IDs when no policy data in store.
TEST(CloudManagementAffiliationTest,
     GetUserAffiliationIdsFromCore_NoPolicyData) {
  base::test::TaskEnvironment task_environment;

  policy::MockUserCloudPolicyStore store;
  policy::CloudPolicyCore core(
      policy::dm_protocol::kChromeUserPolicyType, std::string(), &store,
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      network::TestNetworkConnectionTracker::CreateGetter());

  // Connect and register client.
  policy::MockCloudPolicyClient* client = ConnectNewMockClient(&core);
  client->SetDMToken(kNonEmptyDmToken);

  auto returned_ids = GetAffiliationIdsFromCore(core, /*for_device=*/false);

  EXPECT_THAT(returned_ids, IsEmpty());
}
#endif

}  // namespace policy
