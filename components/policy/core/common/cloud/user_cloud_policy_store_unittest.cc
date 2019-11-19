// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/user_cloud_policy_store.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AllOf;
using testing::Eq;
using testing::Mock;
using testing::Property;
using testing::Sequence;

namespace policy {

namespace {

void RunUntilIdle() {
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

bool WriteStringToFile(const base::FilePath path, const std::string& data) {
  if (!base::CreateDirectory(path.DirName())) {
    DLOG(WARNING) << "Failed to create directory " << path.DirName().value();
    return false;
  }

  int size = data.size();
  if (base::WriteFile(path, data.c_str(), size) != size) {
    DLOG(WARNING) << "Failed to write " << path.value();
    return false;
  }

  return true;
}

}  // namespace

class UserCloudPolicyStoreTest : public testing::Test {
 public:
  UserCloudPolicyStoreTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI) {}

  void SetUp() override {
    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());
    store_.reset(new UserCloudPolicyStore(policy_file(), key_file(),
                                          base::ThreadTaskRunnerHandle::Get()));
    external_data_manager_.reset(new MockCloudExternalDataManager);
    external_data_manager_->SetPolicyStore(store_.get());
    store_->SetSigninAccountId(PolicyBuilder::GetFakeAccountIdForTesting());
    EXPECT_EQ(PolicyBuilder::GetFakeAccountIdForTesting(),
              store_->signin_account_id());
    store_->AddObserver(&observer_);

    // Install an initial public key, so that by default the validation of
    // the stored/loaded policy blob succeeds (it looks like a new key
    // provision).
    policy_.SetDefaultInitialSigningKey();

    InitPolicyPayload(&policy_.payload());

    policy_.Build();
  }

  void TearDown() override {
    store_->RemoveObserver(&observer_);
    external_data_manager_.reset();
    store_.reset();
    RunUntilIdle();
  }

  void InitPolicyPayload(enterprise_management::CloudPolicySettings* payload) {
    payload->mutable_searchsuggestenabled()->set_value(true);
    payload->mutable_urlblacklist()->mutable_value()->add_entries(
        "chromium.org");
  }

  base::FilePath policy_file() {
    return tmp_dir_.GetPath().AppendASCII("policy");
  }

  base::FilePath key_file() {
    return tmp_dir_.GetPath().AppendASCII("policy_key");
  }

  // Verifies that store_->policy_map() has the appropriate entries.
  void VerifyPolicyMap(CloudPolicyStore* store) {
    EXPECT_EQ(2U, store->policy_map().size());
    const PolicyMap::Entry* entry =
        store->policy_map().Get(key::kSearchSuggestEnabled);
    ASSERT_TRUE(entry);
    EXPECT_TRUE(base::Value(true).Equals(entry->value.get()));
    ASSERT_TRUE(store->policy_map().Get(key::kURLBlacklist));
  }

  // Install an expectation on |observer_| for an error code.
  void ExpectError(CloudPolicyStore* store, CloudPolicyStore::Status error) {
    EXPECT_CALL(observer_,
                OnStoreError(AllOf(Eq(store),
                                   Property(&CloudPolicyStore::status,
                                            Eq(error)))));
  }

  void StorePolicyAndEnsureLoaded(
      const enterprise_management::PolicyFetchResponse& policy) {
    Sequence s;
    EXPECT_CALL(*external_data_manager_, OnPolicyStoreLoaded()).InSequence(s);
    EXPECT_CALL(observer_, OnStoreLoaded(store_.get())).InSequence(s);
    store_->Store(policy);
    RunUntilIdle();
    Mock::VerifyAndClearExpectations(external_data_manager_.get());
    Mock::VerifyAndClearExpectations(&observer_);
    ASSERT_TRUE(store_->policy());
  }

  UserPolicyBuilder policy_;
  MockCloudPolicyStoreObserver observer_;
  std::unique_ptr<UserCloudPolicyStore> store_;
  std::unique_ptr<MockCloudExternalDataManager> external_data_manager_;

  base::test::SingleThreadTaskEnvironment task_environment_;

  base::ScopedTempDir tmp_dir_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyStoreTest);
};

TEST_F(UserCloudPolicyStoreTest, LoadWithNoFile) {
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());

  Sequence s;
  EXPECT_CALL(*external_data_manager_, OnPolicyStoreLoaded()).InSequence(s);
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get())).InSequence(s);
  store_->Load();
  RunUntilIdle();

  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
}

TEST_F(UserCloudPolicyStoreTest, LoadWithInvalidFile) {
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());

  // Create a bogus file.
  ASSERT_TRUE(base::CreateDirectory(policy_file().DirName()));
  std::string bogus_data = "bogus_data";
  int size = bogus_data.size();
  ASSERT_EQ(size, base::WriteFile(policy_file(),
                                  bogus_data.c_str(), bogus_data.size()));

  ExpectError(store_.get(), CloudPolicyStore::STATUS_LOAD_ERROR);
  store_->Load();
  RunUntilIdle();

  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
}

TEST_F(UserCloudPolicyStoreTest, LoadImmediatelyWithNoFile) {
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());

  Sequence s;
  EXPECT_CALL(*external_data_manager_, OnPolicyStoreLoaded()).InSequence(s);
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get())).InSequence(s);
  store_->LoadImmediately();  // Should load without running the message loop.

  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
}

TEST_F(UserCloudPolicyStoreTest, LoadImmediatelyWithInvalidFile) {
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());

  // Create a bogus file.
  ASSERT_TRUE(base::CreateDirectory(policy_file().DirName()));
  std::string bogus_data = "bogus_data";
  int size = bogus_data.size();
  ASSERT_EQ(size, base::WriteFile(policy_file(),
                                  bogus_data.c_str(), bogus_data.size()));

  ExpectError(store_.get(), CloudPolicyStore::STATUS_LOAD_ERROR);
  store_->LoadImmediately();  // Should load without running the message loop.

  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
}

// Load file from cache with no key data - should give us a validation error.
TEST_F(UserCloudPolicyStoreTest, ShouldFailToLoadUnsignedPolicy) {
  UserPolicyBuilder unsigned_builder;
  unsigned_builder.UnsetSigningKey();
  InitPolicyPayload(&unsigned_builder.payload());
  unsigned_builder.Build();
  // Policy should be unsigned.
  EXPECT_FALSE(unsigned_builder.policy().has_policy_data_signature());

  // Write policy to disk.
  std::string data;
  ASSERT_TRUE(unsigned_builder.policy().SerializeToString(&data));
  ASSERT_TRUE(base::CreateDirectory(policy_file().DirName()));
  int size = data.size();
  ASSERT_EQ(size, base::WriteFile(policy_file(), data.c_str(), size));

  // Now make sure the data generates a validation error.
  ExpectError(store_.get(), CloudPolicyStore::STATUS_VALIDATION_ERROR);
  store_->LoadImmediately();  // Should load without running the message loop.
  Mock::VerifyAndClearExpectations(&observer_);

  // Now mimic a new policy coming down - this should result in a new key
  // being installed.
  StorePolicyAndEnsureLoaded(policy_.policy());
  EXPECT_EQ(policy_.policy().new_public_key(),
            store_->policy_signature_public_key());
  EXPECT_TRUE(store_->policy()->has_public_key_version());
  EXPECT_TRUE(base::PathExists(key_file()));
}

TEST_F(UserCloudPolicyStoreTest, Store) {
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());

  // Store a simple policy and make sure it ends up as the currently active
  // policy.
  StorePolicyAndEnsureLoaded(policy_.policy());

  // Policy should be decoded and stored.
  EXPECT_EQ(policy_.policy_data().SerializeAsString(),
            store_->policy()->SerializeAsString());
  VerifyPolicyMap(store_.get());
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
}

TEST_F(UserCloudPolicyStoreTest, StoreThenClear) {
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());

  // Store a simple policy and make sure the file exists.
  // policy.
  StorePolicyAndEnsureLoaded(policy_.policy());
  EXPECT_FALSE(store_->policy_map().empty());

  // Policy file should exist.
  ASSERT_TRUE(base::PathExists(policy_file()));

  Sequence s2;
  EXPECT_CALL(*external_data_manager_, OnPolicyStoreLoaded()).InSequence(s2);
  EXPECT_CALL(observer_, OnStoreLoaded(store_.get())).InSequence(s2);
  store_->Clear();
  RunUntilIdle();

  // Policy file should not exist.
  ASSERT_TRUE(!base::PathExists(policy_file()));

  // Policy should be gone.
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
}

TEST_F(UserCloudPolicyStoreTest, StoreRotatedKey) {
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());

  // Store a simple policy and make sure it ends up as the currently active
  // policy.
  StorePolicyAndEnsureLoaded(policy_.policy());
  EXPECT_FALSE(policy_.policy().has_new_public_key_signature());
  std::string original_policy_key = policy_.policy().new_public_key();
  EXPECT_EQ(original_policy_key, store_->policy_signature_public_key());

  // Now do key rotation.
  policy_.SetDefaultSigningKey();
  policy_.SetDefaultNewSigningKey();
  policy_.Build();
  EXPECT_TRUE(policy_.policy().has_new_public_key_signature());
  EXPECT_NE(original_policy_key, policy_.policy().new_public_key());
  StorePolicyAndEnsureLoaded(policy_.policy());
  EXPECT_EQ(policy_.policy().new_public_key(),
            store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreTest, ProvisionKeyTwice) {
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());

  // Store a simple policy and make sure it ends up as the currently active
  // policy.
  StorePolicyAndEnsureLoaded(policy_.policy());

  // Now try sending down policy signed with a different key (i.e. do key
  // rotation with a key not signed with the original signing key).
  policy_.UnsetSigningKey();
  policy_.SetDefaultNewSigningKey();
  policy_.Build();
  EXPECT_FALSE(policy_.policy().has_new_public_key_signature());

  ExpectError(store_.get(), CloudPolicyStore::STATUS_VALIDATION_ERROR);
  store_->Store(policy_.policy());
  RunUntilIdle();
}

TEST_F(UserCloudPolicyStoreTest, StoreTwoTimes) {
  EXPECT_FALSE(store_->policy());
  EXPECT_TRUE(store_->policy_map().empty());

  // Store a simple policy then store a second policy before the first one
  // finishes validating, and make sure the second policy ends up as the active
  // policy.
  UserPolicyBuilder first_policy;
  first_policy.SetDefaultInitialSigningKey();
  first_policy.payload().mutable_searchsuggestenabled()->set_value(false);
  first_policy.Build();
  StorePolicyAndEnsureLoaded(first_policy.policy());

  // Rebuild policy with the same signing key as |first_policy| (no rotation).
  policy_.UnsetNewSigningKey();
  policy_.SetDefaultSigningKey();
  policy_.Build();
  ASSERT_FALSE(policy_.policy().has_new_public_key());
  StorePolicyAndEnsureLoaded(policy_.policy());

  // Policy should be decoded and stored.
  EXPECT_EQ(policy_.policy_data().SerializeAsString(),
            store_->policy()->SerializeAsString());
  VerifyPolicyMap(store_.get());
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
}

TEST_F(UserCloudPolicyStoreTest, StoreThenLoad) {
  // Store a simple policy and make sure it can be read back in.
  // policy.
  StorePolicyAndEnsureLoaded(policy_.policy());
  EXPECT_FALSE(store_->policy_signature_public_key().empty());

  // Now, make sure the policy can be read back in from a second store.
  std::unique_ptr<UserCloudPolicyStore> store2(new UserCloudPolicyStore(
      policy_file(), key_file(), base::ThreadTaskRunnerHandle::Get()));
  store2->SetSigninAccountId(PolicyBuilder::GetFakeAccountIdForTesting());
  store2->AddObserver(&observer_);
  EXPECT_CALL(observer_, OnStoreLoaded(store2.get()));
  store2->Load();
  RunUntilIdle();

  ASSERT_TRUE(store2->policy());
  EXPECT_EQ(policy_.policy_data().SerializeAsString(),
            store2->policy()->SerializeAsString());
  VerifyPolicyMap(store2.get());
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store2->status());
  store2->RemoveObserver(&observer_);
  // Make sure that we properly resurrected the keys.
  EXPECT_EQ(store2->policy_signature_public_key(),
            store_->policy_signature_public_key());
}

TEST_F(UserCloudPolicyStoreTest, StoreThenLoadImmediately) {
  // Store a simple policy and make sure it can be read back in.
  // policy.
  StorePolicyAndEnsureLoaded(policy_.policy());

  // Now, make sure the policy can be read back in from a second store.
  std::unique_ptr<UserCloudPolicyStore> store2(new UserCloudPolicyStore(
      policy_file(), key_file(), base::ThreadTaskRunnerHandle::Get()));
  store2->SetSigninAccountId(PolicyBuilder::GetFakeAccountIdForTesting());
  store2->AddObserver(&observer_);
  EXPECT_CALL(observer_, OnStoreLoaded(store2.get()));
  store2->LoadImmediately();  // Should load without running the message loop.

  ASSERT_TRUE(store2->policy());
  EXPECT_EQ(policy_.policy_data().SerializeAsString(),
            store2->policy()->SerializeAsString());
  VerifyPolicyMap(store2.get());
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store2->status());
  store2->RemoveObserver(&observer_);
}

TEST_F(UserCloudPolicyStoreTest, StoreValidationError) {
  // Create an invalid policy (no policy type).
  policy_.policy_data().clear_policy_type();
  policy_.Build();

  // Store policy.
  ExpectError(store_.get(), CloudPolicyStore::STATUS_VALIDATION_ERROR);
  store_->Store(policy_.policy());
  RunUntilIdle();
  ASSERT_FALSE(store_->policy());
}

TEST_F(UserCloudPolicyStoreTest, StoreUnsigned) {
  // Create unsigned policy, try to store it, should get a validation error.
  policy_.policy().mutable_policy_data_signature()->clear();

  // Store policy.
  ExpectError(store_.get(), CloudPolicyStore::STATUS_VALIDATION_ERROR);
  store_->Store(policy_.policy());
  RunUntilIdle();
  ASSERT_FALSE(store_->policy());
}

TEST_F(UserCloudPolicyStoreTest, LoadValidationError) {
  AccountId other_account_id =
      AccountId::FromUserEmailGaiaId("foobar@foobar.com", "another-gaia-id");
  // Force a validation error by changing the account id after policy is stored.
  StorePolicyAndEnsureLoaded(policy_.policy());

  // Sign out, and sign back in as a different user, and try to load the profile
  // data (should fail due to mismatched account id).
  std::unique_ptr<UserCloudPolicyStore> store2(new UserCloudPolicyStore(
      policy_file(), key_file(), base::ThreadTaskRunnerHandle::Get()));
  store2->SetSigninAccountId(other_account_id);
  store2->AddObserver(&observer_);
  ExpectError(store2.get(), CloudPolicyStore::STATUS_VALIDATION_ERROR);
  store2->Load();
  RunUntilIdle();

  ASSERT_FALSE(store2->policy());
  store2->RemoveObserver(&observer_);

  // Sign out - we should be able to load the policy (don't check users
  // when signed out).
  std::unique_ptr<UserCloudPolicyStore> store3(new UserCloudPolicyStore(
      policy_file(), key_file(), base::ThreadTaskRunnerHandle::Get()));
  store3->AddObserver(&observer_);
  EXPECT_CALL(observer_, OnStoreLoaded(store3.get()));
  store3->Load();
  RunUntilIdle();

  ASSERT_TRUE(store3->policy());
  store3->RemoveObserver(&observer_);

  // Now start a signin as a different user - this should fail validation.
  std::unique_ptr<UserCloudPolicyStore> store4(new UserCloudPolicyStore(
      policy_file(), key_file(), base::ThreadTaskRunnerHandle::Get()));
  store4->SetSigninAccountId(other_account_id);
  store4->AddObserver(&observer_);
  ExpectError(store4.get(), CloudPolicyStore::STATUS_VALIDATION_ERROR);
  store4->Load();
  RunUntilIdle();

  ASSERT_FALSE(store4->policy());
  store4->RemoveObserver(&observer_);
}

TEST_F(UserCloudPolicyStoreTest, KeyRotation) {
  // Make sure when we load data from disk with a different key, that we trigger
  // a server-side key rotation.
  StorePolicyAndEnsureLoaded(policy_.policy());
  ASSERT_TRUE(store_->policy()->has_public_key_version());

  std::string key_data;
  enterprise_management::PolicySigningKey key;
  ASSERT_TRUE(base::ReadFileToString(key_file(), &key_data));
  ASSERT_TRUE(key.ParseFromString(key_data));
  key.set_verification_key("different_key");
  key.SerializeToString(&key_data);
  WriteStringToFile(key_file(), key_data);

  // Now load this in a new store - this should trigger key rotation. The keys
  // will still verify using the existing verification key.
  std::unique_ptr<UserCloudPolicyStore> store2(new UserCloudPolicyStore(
      policy_file(), key_file(), base::ThreadTaskRunnerHandle::Get()));
  store2->SetSigninAccountId(PolicyBuilder::GetFakeAccountIdForTesting());
  store2->AddObserver(&observer_);
  EXPECT_CALL(observer_, OnStoreLoaded(store2.get()));
  store2->Load();
  RunUntilIdle();
  ASSERT_TRUE(store2->policy());
  ASSERT_FALSE(store2->policy()->has_public_key_version());
  store2->RemoveObserver(&observer_);
}

TEST_F(UserCloudPolicyStoreTest, InvalidCachedVerificationSignature) {
  // Make sure that we reject code with an invalid key.
  StorePolicyAndEnsureLoaded(policy_.policy());

  std::string key_data;
  enterprise_management::PolicySigningKey key;
  ASSERT_TRUE(base::ReadFileToString(key_file(), &key_data));
  ASSERT_TRUE(key.ParseFromString(key_data));
  key.set_signing_key_signature("different_key");
  key.SerializeToString(&key_data);
  WriteStringToFile(key_file(), key_data);

  // Now load this in a new store - this should cause a validation error because
  // the key won't verify.
  std::unique_ptr<UserCloudPolicyStore> store2(new UserCloudPolicyStore(
      policy_file(), key_file(), base::ThreadTaskRunnerHandle::Get()));
  store2->SetSigninAccountId(PolicyBuilder::GetFakeAccountIdForTesting());
  store2->AddObserver(&observer_);
  ExpectError(store2.get(), CloudPolicyStore::STATUS_VALIDATION_ERROR);
  store2->Load();
  RunUntilIdle();
  store2->RemoveObserver(&observer_);
}

}  // namespace policy
