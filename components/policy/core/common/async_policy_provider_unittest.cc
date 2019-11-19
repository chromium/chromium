// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/async_policy_provider.h"

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/policy/core/common/async_policy_loader.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema_registry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Mock;
using testing::Return;
using testing::Sequence;

namespace policy {

namespace {

// Helper to write a policy in |bundle| with less code.
void SetPolicy(PolicyBundle* bundle,
               const std::string& name,
               const std::string& value) {
  bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .Set(name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
           POLICY_SOURCE_PLATFORM, std::make_unique<base::Value>(value),
           nullptr);
}

class MockPolicyLoader : public AsyncPolicyLoader {
 public:
  explicit MockPolicyLoader(
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~MockPolicyLoader() override;

  // Load() returns a std::unique_ptr<PolicyBundle> but it can't be mocked
  // because std::unique_ptr is moveable but not copyable. This override
  // forwards the call to MockLoad() which returns a PolicyBundle*, and returns
  // a copy wrapped in a std::unique_ptr.
  std::unique_ptr<PolicyBundle> Load() override;

  MOCK_METHOD0(MockLoad, const PolicyBundle*());
  MOCK_METHOD0(InitOnBackgroundThread, void());
  MOCK_METHOD0(LastModificationTime, base::Time());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPolicyLoader);
};

MockPolicyLoader::MockPolicyLoader(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : AsyncPolicyLoader(task_runner) {}

MockPolicyLoader::~MockPolicyLoader() {}

std::unique_ptr<PolicyBundle> MockPolicyLoader::Load() {
  std::unique_ptr<PolicyBundle> bundle;
  const PolicyBundle* loaded = MockLoad();
  if (loaded) {
    bundle.reset(new PolicyBundle());
    bundle->CopyFrom(*loaded);
  }
  return bundle;
}

}  // namespace

class AsyncPolicyProviderTest : public testing::Test {
 protected:
  AsyncPolicyProviderTest();
  ~AsyncPolicyProviderTest() override;

  void SetUp() override;
  void TearDown() override;

  base::test::SingleThreadTaskEnvironment task_environment_;
  SchemaRegistry schema_registry_;
  PolicyBundle initial_bundle_;
  MockPolicyLoader* loader_;
  std::unique_ptr<AsyncPolicyProvider> provider_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AsyncPolicyProviderTest);
};

AsyncPolicyProviderTest::AsyncPolicyProviderTest() {}

AsyncPolicyProviderTest::~AsyncPolicyProviderTest() {}

void AsyncPolicyProviderTest::SetUp() {
  SetPolicy(&initial_bundle_, "policy", "initial");
  loader_ = new MockPolicyLoader(base::ThreadTaskRunnerHandle::Get());
  EXPECT_CALL(*loader_, LastModificationTime())
      .WillRepeatedly(Return(base::Time()));
  EXPECT_CALL(*loader_, InitOnBackgroundThread()).Times(1);
  EXPECT_CALL(*loader_, MockLoad()).WillOnce(Return(&initial_bundle_));

  provider_.reset(new AsyncPolicyProvider(
      &schema_registry_, std::unique_ptr<AsyncPolicyLoader>(loader_)));
  provider_->Init(&schema_registry_);
  // Verify that the initial load is done synchronously:
  EXPECT_TRUE(provider_->policies().Equals(initial_bundle_));

  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(loader_);

  EXPECT_CALL(*loader_, LastModificationTime())
      .WillRepeatedly(Return(base::Time()));
}

void AsyncPolicyProviderTest::TearDown() {
  if (provider_) {
    provider_->Shutdown();
    provider_.reset();
  }
  base::RunLoop().RunUntilIdle();
}

TEST_F(AsyncPolicyProviderTest, RefreshPolicies) {
  PolicyBundle refreshed_bundle;
  SetPolicy(&refreshed_bundle, "policy", "refreshed");
  EXPECT_CALL(*loader_, MockLoad()).WillOnce(Return(&refreshed_bundle));

  MockConfigurationPolicyObserver observer;
  provider_->AddObserver(&observer);
  EXPECT_CALL(observer, OnUpdatePolicy(provider_.get())).Times(1);
  provider_->RefreshPolicies();
  base::RunLoop().RunUntilIdle();
  // The refreshed policies are now provided.
  EXPECT_TRUE(provider_->policies().Equals(refreshed_bundle));
  provider_->RemoveObserver(&observer);
}

TEST_F(AsyncPolicyProviderTest, RefreshPoliciesTwice) {
  PolicyBundle refreshed_bundle;
  SetPolicy(&refreshed_bundle, "policy", "refreshed");
  EXPECT_CALL(*loader_, MockLoad()).WillRepeatedly(Return(&refreshed_bundle));

  MockConfigurationPolicyObserver observer;
  provider_->AddObserver(&observer);
  EXPECT_CALL(observer, OnUpdatePolicy(provider_.get())).Times(0);
  provider_->RefreshPolicies();
  // Doesn't refresh before going through the background thread.
  Mock::VerifyAndClearExpectations(&observer);

  // Doesn't refresh if another RefreshPolicies request is made.
  EXPECT_CALL(observer, OnUpdatePolicy(provider_.get())).Times(0);
  provider_->RefreshPolicies();
  Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnUpdatePolicy(provider_.get())).Times(1);
  base::RunLoop().RunUntilIdle();
  // The refreshed policies are now provided.
  EXPECT_TRUE(provider_->policies().Equals(refreshed_bundle));
  Mock::VerifyAndClearExpectations(&observer);
  provider_->RemoveObserver(&observer);
}

TEST_F(AsyncPolicyProviderTest, RefreshPoliciesDuringReload) {
  PolicyBundle reloaded_bundle;
  SetPolicy(&reloaded_bundle, "policy", "reloaded");
  PolicyBundle refreshed_bundle;
  SetPolicy(&refreshed_bundle, "policy", "refreshed");

  Sequence load_sequence;
  // Reload.
  EXPECT_CALL(*loader_, MockLoad()).InSequence(load_sequence)
                                   .WillOnce(Return(&reloaded_bundle));
  // RefreshPolicies.
  EXPECT_CALL(*loader_, MockLoad()).InSequence(load_sequence)
                                   .WillOnce(Return(&refreshed_bundle));

  MockConfigurationPolicyObserver observer;
  provider_->AddObserver(&observer);
  EXPECT_CALL(observer, OnUpdatePolicy(provider_.get())).Times(0);

  // A Reload is triggered before RefreshPolicies, and it shouldn't trigger
  // notifications.
  loader_->Reload(true);
  Mock::VerifyAndClearExpectations(&observer);

  // Doesn't refresh before going through the background thread.
  EXPECT_CALL(observer, OnUpdatePolicy(provider_.get())).Times(0);
  provider_->RefreshPolicies();
  Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnUpdatePolicy(provider_.get())).Times(1);
  base::RunLoop().RunUntilIdle();
  // The refreshed policies are now provided, and the |reloaded_bundle| was
  // dropped.
  EXPECT_TRUE(provider_->policies().Equals(refreshed_bundle));
  Mock::VerifyAndClearExpectations(&observer);
  provider_->RemoveObserver(&observer);
}

TEST_F(AsyncPolicyProviderTest, Shutdown) {
  EXPECT_CALL(*loader_, MockLoad()).WillRepeatedly(Return(&initial_bundle_));

  MockConfigurationPolicyObserver observer;
  provider_->AddObserver(&observer);

  // Though there is a pending Reload, the provider and the loader can be
  // deleted at any time.
  EXPECT_CALL(observer, OnUpdatePolicy(provider_.get())).Times(0);
  loader_->Reload(true);
  Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnUpdatePolicy(provider_.get())).Times(0);
  provider_->Shutdown();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  provider_->RemoveObserver(&observer);
  provider_.reset();
}

}  // namespace policy
