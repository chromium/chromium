// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/groups/enterprise_groups_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/enterprise/browser/groups/groups_features.h"
#include "components/enterprise/browser/groups/groups_prefs.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class EnterpriseGroupsHandlerTest : public testing::Test {
 public:
  EnterpriseGroupsHandlerTest(const EnterpriseGroupsHandlerTest&) = delete;
  EnterpriseGroupsHandlerTest& operator=(const EnterpriseGroupsHandlerTest&) =
      delete;

 protected:
  EnterpriseGroupsHandlerTest()
      : policy_type_(dm_protocol::GetChromeUserPolicyType()) {}

  void SetUp() override {
    local_state_.registry()->RegisterListPref(
        enterprise_groups::kEnterpriseGroupsBrowserPref);
    local_state_.registry()->RegisterDictionaryPref(
        enterprise_groups::kEnterpriseGroupsProfilePref);

    auto store = std::make_unique<MockCloudPolicyStore>(
        dm_protocol::GetChromeUserPolicyType());
    store_ = store.get();
    EXPECT_CALL(*store_, Load());
    manager_ = std::make_unique<MockCloudPolicyManager>(
        std::move(store), std::unique_ptr<MockCloudPolicyStore>(),
        task_environment_.GetMainThreadTaskRunner());
    manager_->Init(&schema_registry_);
    MockCloudPolicyClient* client = new MockCloudPolicyClient();
    manager_->core()->Connect(std::unique_ptr<CloudPolicyClient>(client));
  }

  void TearDown() override { manager_->Shutdown(); }

  // Needs to be the first member.
  base::test::TaskEnvironment task_environment_;

  TestingPrefServiceSimple local_state_;
  const std::string policy_type_;
  UserPolicyBuilder policy_;
  SchemaRegistry schema_registry_;
  std::unique_ptr<MockCloudPolicyManager> manager_;
  raw_ptr<MockCloudPolicyStore> store_;
};

std::unique_ptr<enterprise_management::PolicyData>
CreatePolicyDataWithAvailableGroupIds(base::span<const std::string> group_ids) {
  UserPolicyBuilder policy_builder;
  for (const std::string& group_id : group_ids) {
    policy_builder.policy_data().add_available_group_ids(group_id);
  }
  policy_builder.Build();
  return std::make_unique<enterprise_management::PolicyData>(
      policy_builder.policy_data());
}

void SetEnterpriseGroupsForProfile(PrefService* local_state,
                                   const std::string& profile_key,
                                   base::span<const std::string> group_ids) {
  ScopedDictPrefUpdate groups_prefs_update(
      local_state, enterprise_groups::kEnterpriseGroupsProfilePref);
  base::ListValue groups;
  for (const std::string& group_id : group_ids) {
    groups.Append(group_id);
  }
  groups_prefs_update->Set(profile_key, std::move(groups));
}

TEST_F(EnterpriseGroupsHandlerTest, UpdatesEnterpriseGroupsForBrowser) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_groups::kEnterpriseGroupsExperiments);

  EnterpriseGroupsBrowserHandler handler(manager_->core(), &local_state_);
  handler.Init();
  store_->set_policy_data_for_testing(
      CreatePolicyDataWithAvailableGroupIds({"group_id1", "group_id2"}));

  store_->NotifyStoreLoaded();

  EXPECT_EQ(
      local_state_.GetList(enterprise_groups::kEnterpriseGroupsBrowserPref),
      base::ListValue().Append("group_id1").Append("group_id2"));
}

TEST_F(EnterpriseGroupsHandlerTest,
       DoesNotUpdateEnterpriseGroupsForBrowserIfFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      enterprise_groups::kEnterpriseGroupsExperiments);

  EnterpriseGroupsBrowserHandler handler(manager_->core(), &local_state_);
  handler.Init();
  store_->set_policy_data_for_testing(
      CreatePolicyDataWithAvailableGroupIds({"group_id1", "group_id2"}));

  store_->NotifyStoreLoaded();

  EXPECT_THAT(
      local_state_.GetList(enterprise_groups::kEnterpriseGroupsBrowserPref),
      ::testing::IsEmpty());
}

TEST_F(EnterpriseGroupsHandlerTest,
       ClearsEnterpriseGroupsForBrowserIfPolicyHasNoGroupIds) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_groups::kEnterpriseGroupsExperiments);
  local_state_.SetList(
      enterprise_groups::kEnterpriseGroupsBrowserPref,
      base::ListValue().Append("group_id1").Append("group_id2"));

  EnterpriseGroupsBrowserHandler handler(manager_->core(), &local_state_);
  handler.Init();
  store_->set_policy_data_for_testing(
      CreatePolicyDataWithAvailableGroupIds({}));

  store_->NotifyStoreLoaded();
  EXPECT_THAT(
      local_state_.GetList(enterprise_groups::kEnterpriseGroupsBrowserPref),
      ::testing::IsEmpty());
}

TEST_F(EnterpriseGroupsHandlerTest,
       ClearsEnterpriseGroupsForBrowserIfPolicyIsNull) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_groups::kEnterpriseGroupsExperiments);
  local_state_.SetList(
      enterprise_groups::kEnterpriseGroupsBrowserPref,
      base::ListValue().Append("group_id1").Append("group_id2"));

  EnterpriseGroupsBrowserHandler handler(manager_->core(), &local_state_);
  handler.Init();

  store_->SetFirstPoliciesLoaded(true);
  store_->NotifyStoreLoaded();
  EXPECT_THAT(
      local_state_.GetList(enterprise_groups::kEnterpriseGroupsBrowserPref),
      ::testing::IsEmpty());
}

TEST_F(EnterpriseGroupsHandlerTest, ClearsEnterpriseGroupsForBrowser) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_groups::kEnterpriseGroupsExperiments);
  local_state_.SetList(
      enterprise_groups::kEnterpriseGroupsBrowserPref,
      base::ListValue().Append("group_id1").Append("group_id2"));

  EnterpriseGroupsBrowserHandler handler(manager_->core(), &local_state_);
  // ClearGroups can be called without initializing the handler.
  handler.ClearGroups();

  EXPECT_THAT(
      local_state_.GetList(enterprise_groups::kEnterpriseGroupsBrowserPref),
      ::testing::IsEmpty());
}

TEST_F(EnterpriseGroupsHandlerTest, UpdatesEnterpriseGroupsForProfile) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_groups::kEnterpriseGroupsExperiments);

  EnterpriseGroupsProfileHandler handler(manager_->core(), &local_state_,
                                         "profile_key");
  handler.Init();
  store_->set_policy_data_for_testing(
      CreatePolicyDataWithAvailableGroupIds({"group_id1", "group_id2"}));

  store_->NotifyStoreLoaded();

  EXPECT_TRUE(handler.IsObserving());
  EXPECT_EQ(
      *local_state_.GetDict(enterprise_groups::kEnterpriseGroupsProfilePref)
           .FindList("profile_key"),
      base::ListValue().Append("group_id1").Append("group_id2"));
}

TEST_F(EnterpriseGroupsHandlerTest,
       ClearsEnterpriseGroupsForProfileIfPolicyIsNull) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_groups::kEnterpriseGroupsExperiments);
  SetEnterpriseGroupsForProfile(&local_state_, "profile_key",
                                {"group_id1", "group_id2"});

  EnterpriseGroupsProfileHandler handler(manager_->core(), &local_state_,
                                         "profile_key");
  handler.Init();

  store_->SetFirstPoliciesLoaded(true);
  store_->NotifyStoreLoaded();
  EXPECT_THAT(
      local_state_.GetDict(enterprise_groups::kEnterpriseGroupsProfilePref),
      ::testing::IsEmpty());
}

TEST_F(EnterpriseGroupsHandlerTest,
       ClearsEnterpriseGroupsForProfileIfPolicyHasNoGroupIds) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_groups::kEnterpriseGroupsExperiments);
  SetEnterpriseGroupsForProfile(&local_state_, "profile_key",
                                {"group_id1", "group_id2"});

  EnterpriseGroupsProfileHandler handler(manager_->core(), &local_state_,
                                         "profile_key");
  handler.Init();
  store_->set_policy_data_for_testing(
      CreatePolicyDataWithAvailableGroupIds({}));

  store_->NotifyStoreLoaded();
  EXPECT_THAT(
      local_state_.GetDict(enterprise_groups::kEnterpriseGroupsProfilePref),
      ::testing::IsEmpty());
}

TEST_F(EnterpriseGroupsHandlerTest,
       ClearsGroupsAndStopsObservingOnResetAndClearGroups) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_groups::kEnterpriseGroupsExperiments);
  SetEnterpriseGroupsForProfile(&local_state_, "profile_key",
                                {"group_id1", "group_id2"});

  EnterpriseGroupsProfileHandler handler(manager_->core(), &local_state_,
                                         "profile_key");
  handler.Init();
  handler.ResetAndClearGroups();

  store_->set_policy_data_for_testing(
      CreatePolicyDataWithAvailableGroupIds({"group_id3"}));
  store_->NotifyStoreLoaded();

  EXPECT_FALSE(handler.IsObserving());
  EXPECT_THAT(
      local_state_.GetDict(enterprise_groups::kEnterpriseGroupsProfilePref),
      ::testing::IsEmpty());
}

TEST_F(EnterpriseGroupsHandlerTest,
       DoesNotUpdateEnterpriseGroupsAfterShutdown) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_groups::kEnterpriseGroupsExperiments);

  EnterpriseGroupsProfileHandler handler(manager_->core(), &local_state_,
                                         "profile_key");
  handler.Init();

  store_->set_policy_data_for_testing(
      CreatePolicyDataWithAvailableGroupIds({"group_id1"}));
  store_->NotifyStoreLoaded();

  handler.Shutdown();

  store_->set_policy_data_for_testing(
      CreatePolicyDataWithAvailableGroupIds({"group_id2"}));
  store_->NotifyStoreLoaded();

  EXPECT_EQ(
      *local_state_.GetDict(enterprise_groups::kEnterpriseGroupsProfilePref)
           .FindList("profile_key"),
      base::ListValue().Append("group_id1"));
}

}  // namespace policy
