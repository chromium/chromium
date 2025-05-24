// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_store.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/policy_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "base/logging.h"

using ::testing::_;

namespace policy {
namespace {
constexpr int kPublicKeyVersion = 0;
}

// The unit test for MachineLevelUserCloudPolicyStore. Note that most of test
// cases are covered by UserCloudPolicyStoreTest so that the cases here are
// focus on testing the unique part of MachineLevelUserCloudPolicyStore.
class MachineLevelUserCloudPolicyStoreTest : public ::testing::Test {
 public:
  MachineLevelUserCloudPolicyStoreTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {
    policy_.SetDefaultInitialSigningKey();
    policy_.policy_data().set_policy_type(
        dm_protocol::kChromeMachineLevelUserCloudPolicyType);
    policy_.payload().mutable_searchsuggestenabled()->set_value(false);
    policy_.Build();
  }
  MachineLevelUserCloudPolicyStoreTest(
      const MachineLevelUserCloudPolicyStoreTest&) = delete;
  MachineLevelUserCloudPolicyStoreTest& operator=(
      const MachineLevelUserCloudPolicyStoreTest&) = delete;

  ~MachineLevelUserCloudPolicyStoreTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(tmp_policy_dir_.CreateUniqueTempDir());
    updater_policy_dir_ =
        tmp_policy_dir_.GetPath().AppendASCII("updater_policies");
    store_ = CreateStore();
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(
        switches::kPolicyVerificationKey,
        PolicyBuilder::GetEncodedPolicyVerificationKey());
  }

  void SetExpectedPolicyMap(PolicySource source) {
    expected_policy_map_.Clear();
    expected_policy_map_.Set("SearchSuggestEnabled", POLICY_LEVEL_MANDATORY,
                             POLICY_SCOPE_MACHINE, source, base::Value(false),
                             nullptr);
  }

  std::unique_ptr<MachineLevelUserCloudPolicyStore> CreateStore() {
    std::unique_ptr<MachineLevelUserCloudPolicyStore> store =
        MachineLevelUserCloudPolicyStore::Create(
            DMToken::CreateValidToken(PolicyBuilder::kFakeToken),
            PolicyBuilder::kFakeDeviceId, updater_policy_dir_,
            tmp_policy_dir_.GetPath(),
            base::SingleThreadTaskRunner::GetCurrentDefault());
    store->AddObserver(&observer_);
    return store;
  }

  void TearDown() override {
    store_->RemoveObserver(&observer_);
    store_.reset();
    base::RunLoop().RunUntilIdle();
  }

  const base::FilePath updater_policy_cache_path() const {
    return updater_policy_dir_
        .AppendASCII(
            policy::dm_protocol::kChromeMachineLevelUserCloudPolicyTypeBase64)
        .AppendASCII("PolicyFetchResponse");
  }

  const base::FilePath updater_policy_info_path() const {
    return updater_policy_dir_.AppendASCII("CachedPolicyInfo");
  }

  void StorePolicyInUpdaterPath(
      const enterprise_management::PolicyFetchResponse& policy) {
    ASSERT_TRUE(base::CreateDirectory(updater_policy_cache_path().DirName()));
    EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
    store_->Store(policy);
    base::RunLoop().RunUntilIdle();
    ::testing::Mock::VerifyAndClearExpectations(&observer_);
    base::FilePath policy_path = tmp_policy_dir_.GetPath().AppendASCII(
        "Machine Level User Cloud Policy");
    ASSERT_TRUE(base::CopyFile(policy_path, updater_policy_info_path()));
    ASSERT_TRUE(base::Move(policy_path, updater_policy_cache_path()));
  }

  std::unique_ptr<MachineLevelUserCloudPolicyStore> store_;

  base::ScopedTempDir tmp_policy_dir_;
  base::FilePath updater_policy_dir_;
  UserPolicyBuilder policy_;
  UserPolicyBuilder updater_policy_;
  MockCloudPolicyStoreObserver observer_;
  PolicyMap expected_policy_map_;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(MachineLevelUserCloudPolicyStoreTest, LoadWithoutDMToken) {
  store_->SetupRegistration(DMToken::CreateEmptyToken(), std::string());
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());

  EXPECT_CALL(observer_, OnStoreLoaded(_)).Times(0);
  EXPECT_CALL(observer_, OnStoreError(_)).Times(0);

  store_->Load();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());

  ::testing::Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(MachineLevelUserCloudPolicyStoreTest, LoadImmediatelyWithoutDMToken) {
  store_->SetupRegistration(DMToken::CreateEmptyToken(), std::string());
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());

#if BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(observer_, OnStoreLoaded(_)).Times(1);
#else
  EXPECT_CALL(observer_, OnStoreLoaded(_)).Times(0);
#endif
  EXPECT_CALL(observer_, OnStoreError(_)).Times(0);

  store_->LoadImmediately();

  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());

  ::testing::Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(MachineLevelUserCloudPolicyStoreTest, LoadWithNoFile) {
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());

  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
  store_->Load();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());

  ::testing::Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(MachineLevelUserCloudPolicyStoreTest, StorePolicy) {
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
  const base::FilePath policy_path = tmp_policy_dir_.GetPath().Append(
      FILE_PATH_LITERAL("Machine Level User Cloud Policy"));
  const base::FilePath signing_key_path = tmp_policy_dir_.GetPath().Append(
      FILE_PATH_LITERAL("Machine Level User Cloud Policy Signing Key"));
  EXPECT_FALSE(base::PathExists(policy_path));
  EXPECT_FALSE(base::PathExists(signing_key_path));

  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));

  store_->Store(policy_.policy());
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(store_->policy());
  EXPECT_EQ(policy_.policy_data().SerializeAsString(),
            store_->policy()->SerializeAsString());
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
  EXPECT_TRUE(base::PathExists(policy_path));
  EXPECT_TRUE(base::PathExists(signing_key_path));

  ::testing::Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(MachineLevelUserCloudPolicyStoreTest, StoreThenLoadPolicy) {
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
  store_->Store(policy_.policy());
  base::RunLoop().RunUntilIdle();

  ::testing::Mock::VerifyAndClearExpectations(&observer_);

  std::unique_ptr<MachineLevelUserCloudPolicyStore> loader = CreateStore();
  EXPECT_CALL(observer_, OnStoreLoaded(loader.get()));
  loader->Load();
  base::RunLoop().RunUntilIdle();

  SetExpectedPolicyMap(POLICY_SOURCE_CLOUD);
  ASSERT_TRUE(loader->policy());
  EXPECT_EQ(policy_.policy_data().SerializeAsString(),
            loader->policy()->SerializeAsString());
  EXPECT_TRUE(expected_policy_map_.Equals(loader->policy_map()));
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, loader->status());
  loader->RemoveObserver(&observer_);

  ::testing::Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(MachineLevelUserCloudPolicyStoreTest, LoadOlderExternalPolicies) {
  // Create a fake updater policy file.
  policy_.policy_data().set_timestamp(1000);
  policy_.payload().mutable_searchsuggestenabled()->set_value(true);
  policy_.Build();
  StorePolicyInUpdaterPath(policy_.policy());

  // Create a policy file made by the browser.
  std::unique_ptr<MachineLevelUserCloudPolicyStore> store = CreateStore();
  EXPECT_CALL(observer_, OnStoreLoaded(store.get()));
  policy_.policy_data().set_timestamp(2000);
  policy_.policy_data().set_public_key_version(kPublicKeyVersion);
  policy_.payload().mutable_searchsuggestenabled()->set_value(false);
  policy_.Build();
  store->Store(policy_.policy());
  base::RunLoop().RunUntilIdle();
  store->RemoveObserver(&observer_);
  ::testing::Mock::VerifyAndClearExpectations(&observer_);

  // Load the policies and expect to have the updater ones.
  std::unique_ptr<MachineLevelUserCloudPolicyStore> loader = CreateStore();
  EXPECT_CALL(observer_, OnStoreLoaded(loader.get()));
  loader->Load();
  base::RunLoop().RunUntilIdle();

  SetExpectedPolicyMap(POLICY_SOURCE_CLOUD);
  ASSERT_TRUE(loader->policy());
  EXPECT_TRUE(expected_policy_map_.Equals(loader->policy_map()));
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, loader->status());
  loader->RemoveObserver(&observer_);

  // Always keep the key when using the browser policy cache.
  EXPECT_TRUE(loader->policy()->has_public_key_version());
  EXPECT_EQ(kPublicKeyVersion, loader->policy()->public_key_version());

  ::testing::Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(MachineLevelUserCloudPolicyStoreTest, LoadRecentExternalPolicies) {
  // Create a fake updater policy file.
  policy_.policy_data().set_timestamp(2000);
  policy_.payload().mutable_searchsuggestenabled()->set_value(true);
  policy_.policy_data().set_public_key_version(kPublicKeyVersion);
  policy_.Build();
  StorePolicyInUpdaterPath(policy_.policy());

  // Create a policy file made by the browser.
  std::unique_ptr<MachineLevelUserCloudPolicyStore> store = CreateStore();
  EXPECT_CALL(observer_, OnStoreLoaded(store.get()));
  policy_.policy_data().set_timestamp(1000);
  policy_.payload().mutable_searchsuggestenabled()->set_value(false);
  policy_.policy_data().set_public_key_version(kPublicKeyVersion);
  policy_.Build();
  store->Store(policy_.policy());
  base::RunLoop().RunUntilIdle();
  store->RemoveObserver(&observer_);
  ::testing::Mock::VerifyAndClearExpectations(&observer_);

  // Load the policies and expect to have the updater ones.
  std::unique_ptr<MachineLevelUserCloudPolicyStore> loader = CreateStore();
  EXPECT_CALL(observer_, OnStoreLoaded(loader.get()));
  loader->Load();
  base::RunLoop().RunUntilIdle();

  PolicyMap expected_updater_policy_map;
  expected_updater_policy_map.Set(
      "SearchSuggestEnabled", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
      POLICY_SOURCE_CLOUD, base::Value(true), nullptr);

  ASSERT_TRUE(loader->policy());
  EXPECT_TRUE(expected_updater_policy_map.Equals(loader->policy_map()));
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, loader->status());
  loader->RemoveObserver(&observer_);

  // Using updater's key with policy data. However, we won't refresh the
  // key as they are same.
  EXPECT_TRUE(loader->policy()->has_public_key_version());
  EXPECT_EQ(kPublicKeyVersion, loader->policy()->public_key_version());

  ::testing::Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(MachineLevelUserCloudPolicyStoreTest,
       LoadExternalPoliciesAndRefreshKey) {
  // Create a fake updater policy file.
  policy_.policy_data().set_timestamp(2000);
  policy_.payload().mutable_searchsuggestenabled()->set_value(true);
  policy_.policy_data().set_public_key_version(kPublicKeyVersion);
  policy_.SetDefaultNewSigningKey();
  policy_.Build();
  StorePolicyInUpdaterPath(policy_.policy());

  // Create a policy file made by the browser.
  std::unique_ptr<MachineLevelUserCloudPolicyStore> store = CreateStore();
  EXPECT_CALL(observer_, OnStoreLoaded(store.get()));
  policy_.policy_data().set_timestamp(1000);
  policy_.payload().mutable_searchsuggestenabled()->set_value(false);
  policy_.policy_data().set_public_key_version(kPublicKeyVersion);
  policy_.SetDefaultInitialSigningKey();
  policy_.Build();
  store->Store(policy_.policy());
  base::RunLoop().RunUntilIdle();
  store->RemoveObserver(&observer_);
  ::testing::Mock::VerifyAndClearExpectations(&observer_);

  // Load the policies and expect to have the updater ones.
  std::unique_ptr<MachineLevelUserCloudPolicyStore> loader = CreateStore();
  EXPECT_CALL(observer_, OnStoreLoaded(loader.get()));
  loader->Load();
  base::RunLoop().RunUntilIdle();

  PolicyMap expected_updater_policy_map;
  expected_updater_policy_map.Set(
      "SearchSuggestEnabled", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
      POLICY_SOURCE_CLOUD, base::Value(true), nullptr);

  ASSERT_TRUE(loader->policy());
  EXPECT_TRUE(expected_updater_policy_map.Equals(loader->policy_map()));
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, loader->status());
  loader->RemoveObserver(&observer_);

  // Using updater's key with policy data and requires a refresh.
  EXPECT_FALSE(loader->policy()->has_public_key_version());

  ::testing::Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(MachineLevelUserCloudPolicyStoreTest, LoadOnlyExternalPolicies) {
  // Create a fake updater policy file.
  policy_.policy_data().set_timestamp(2000);
  policy_.policy_data().set_public_key_version(kPublicKeyVersion);
  policy_.payload().mutable_searchsuggestenabled()->set_value(true);
  policy_.Build();
  StorePolicyInUpdaterPath(policy_.policy());

  // Load the policies and expect to have the updater ones.
  std::unique_ptr<MachineLevelUserCloudPolicyStore> loader = CreateStore();
  EXPECT_CALL(observer_, OnStoreLoaded(loader.get()));
  loader->Load();
  base::RunLoop().RunUntilIdle();

  PolicyMap expected_updater_policy_map;
  expected_updater_policy_map.Set(
      "SearchSuggestEnabled", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
      POLICY_SOURCE_CLOUD, base::Value(true), nullptr);

  ASSERT_TRUE(loader->policy());
  EXPECT_TRUE(expected_updater_policy_map.Equals(loader->policy_map()));
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, loader->status());

  // Always requires the key refresh when browser policy cache is not available.
  EXPECT_FALSE(loader->policy()->has_public_key_version());

  ::testing::Mock::VerifyAndClearExpectations(&observer_);
  loader->RemoveObserver(&observer_);
}

TEST_F(MachineLevelUserCloudPolicyStoreTest,
       StoreAndLoadWithInvalidTokenPolicy) {
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));
  store_->Store(policy_.policy());
  base::RunLoop().RunUntilIdle();

  ::testing::Mock::VerifyAndClearExpectations(&observer_);

  std::unique_ptr<MachineLevelUserCloudPolicyStore> loader = CreateStore();
  loader->SetupRegistration(DMToken::CreateValidToken("bad_token"),
                            "invalid_client_id");
  EXPECT_CALL(observer_, OnStoreError(loader.get()));
  loader->Load();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(loader->policy());
  EXPECT_TRUE(loader->policy_map().empty());
  EXPECT_EQ(CloudPolicyStore::STATUS_VALIDATION_ERROR, loader->status());
  loader->RemoveObserver(&observer_);

  ::testing::Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(MachineLevelUserCloudPolicyStoreTest, KeyRotation) {
  EXPECT_FALSE(policy_.policy().has_new_public_key_signature());
  std::string original_policy_key = policy_.policy().new_public_key();

  // Store the original key
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));

  store_->Store(policy_.policy());
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(store_->policy());
  EXPECT_EQ(original_policy_key, store_->policy_signature_public_key());

  ::testing::Mock::VerifyAndClearExpectations(&observer_);

  // Store the new key.
  policy_.SetDefaultSigningKey();
  policy_.SetDefaultNewSigningKey();
  policy_.Build();
  EXPECT_TRUE(policy_.policy().has_new_public_key_signature());
  EXPECT_NE(original_policy_key, policy_.policy().new_public_key());

  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));

  store_->Store(policy_.policy());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(policy_.policy().new_public_key(),
            store_->policy_signature_public_key());

  ::testing::Mock::VerifyAndClearExpectations(&observer_);
}

}  // namespace policy
