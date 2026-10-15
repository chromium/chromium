// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/async_policy_loader.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/common/async_policy_provider.h"
#include "components/policy/core/common/management/platform_management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_loader_common.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

// Whether policy loaders on this platform filter sensitive policies when the
// device is not managed.
constexpr bool kFilteringEnabled = BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC);

class MockAsyncPolicyLoader : public AsyncPolicyLoader {
 public:
  MockAsyncPolicyLoader(scoped_refptr<base::SequencedTaskRunner> task_runner,
                        ManagementService* management_service,
                        bool periodic_updates = false)
      : AsyncPolicyLoader(std::move(task_runner),
                          management_service,
                          periodic_updates) {
    ON_CALL(*this, Load).WillByDefault([this]() {
      PolicyBundle bundle = policy_bundle_to_load_.Clone();
      if (ShouldFilterSensitivePolicies()) {
        FilterSensitivePolicies(
            &bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string())));
      }
      return bundle;
    });
  }
  ~MockAsyncPolicyLoader() override = default;

  MOCK_METHOD(PolicyBundle, Load, (), (override));
  MOCK_METHOD(void, InitOnBackgroundThread, (), (override));

  void SetPolicyBundleToLoad(PolicyBundle bundle) {
    policy_bundle_to_load_ = std::move(bundle);
  }

 private:
  PolicyBundle policy_bundle_to_load_;
};

// Returns a bundle with the sensitive DefaultSearchProviderEnabled policy set.
PolicyBundle CreateSensitivePolicyBundle() {
  PolicyBundle bundle;
  bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .Set(key::kDefaultSearchProviderEnabled, POLICY_LEVEL_MANDATORY,
           POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM, base::Value(true),
           nullptr);
  return bundle;
}

bool HasSensitivePolicy(const PolicyBundle& bundle) {
  const base::Value* value =
      bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
          .GetValue(key::kDefaultSearchProviderEnabled,
                    base::Value::Type::BOOLEAN);
  return value && value->GetBool();
}

}  // namespace

class AsyncPolicyLoaderTest : public testing::Test {
 protected:
  AsyncPolicyLoaderTest() = default;
  ~AsyncPolicyLoaderTest() override = default;

  void TearDown() override { task_environment_.RunUntilIdle(); }

  // Runs InitialLoad() on a loader that loads the sensitive policy bundle.
  PolicyBundle RunInitialLoad() {
    MockAsyncPolicyLoader loader(task_environment_.GetMainThreadTaskRunner(),
                                 PlatformManagementService::GetInstance());
    loader.SetPolicyBundleToLoad(CreateSensitivePolicyBundle());
    EXPECT_CALL(loader, Load());
    return loader.InitialLoad(schema_registry_.schema_map());
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  SchemaRegistry schema_registry_;
};

TEST_F(AsyncPolicyLoaderTest, InitialLoad_SensitivePoliciesHonoredWhenManaged) {
  ScopedManagementServiceOverrideForTesting scoped_override(
      PlatformManagementService::GetInstance(),
      EnterpriseManagementAuthority::DOMAIN_LOCAL);

  EXPECT_TRUE(HasSensitivePolicy(RunInitialLoad()));
}

TEST_F(AsyncPolicyLoaderTest,
       InitialLoad_SensitivePoliciesFilteredWhenNotManaged) {
  ScopedManagementServiceOverrideForTesting scoped_override(
      PlatformManagementService::GetInstance(),
      EnterpriseManagementAuthority::NONE);

  EXPECT_EQ(!kFilteringEnabled, HasSensitivePolicy(RunInitialLoad()));
}

TEST_F(AsyncPolicyLoaderTest, Reload_PicksUpManagementStateChanges) {
  auto loader = std::make_unique<MockAsyncPolicyLoader>(
      task_environment_.GetMainThreadTaskRunner(),
      PlatformManagementService::GetInstance());
  MockAsyncPolicyLoader* raw_loader = loader.get();
  raw_loader->SetPolicyBundleToLoad(CreateSensitivePolicyBundle());

  ScopedManagementServiceOverrideForTesting unmanaged_override(
      PlatformManagementService::GetInstance(),
      EnterpriseManagementAuthority::NONE);

  EXPECT_CALL(*raw_loader, InitOnBackgroundThread());
  EXPECT_CALL(*raw_loader, Load()).Times(3);

  AsyncPolicyProvider provider(&schema_registry_, std::move(loader));
  provider.Init(&schema_registry_);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(!kFilteringEnabled, HasSensitivePolicy(provider.policies()));

  // The device becomes managed.
  {
    ScopedManagementServiceOverrideForTesting managed_override(
        PlatformManagementService::GetInstance(),
        EnterpriseManagementAuthority::DOMAIN_LOCAL);
    provider.RefreshPolicies(PolicyFetchReason::kTest);
    task_environment_.RunUntilIdle();
    EXPECT_TRUE(HasSensitivePolicy(provider.policies()));
  }

  // The device is no longer managed: `managed_override` restored the
  // unmanaged state.
  provider.RefreshPolicies(PolicyFetchReason::kTest);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(!kFilteringEnabled, HasSensitivePolicy(provider.policies()));

  provider.Shutdown();
  task_environment_.RunUntilIdle();
}

TEST_F(AsyncPolicyLoaderTest, PeriodicReload_QueriesManagementService) {
  ScopedManagementServiceOverrideForTesting scoped_override(
      PlatformManagementService::GetInstance(),
      EnterpriseManagementAuthority::DOMAIN_LOCAL);

  auto loader = std::make_unique<MockAsyncPolicyLoader>(
      task_environment_.GetMainThreadTaskRunner(),
      PlatformManagementService::GetInstance(),
      /*periodic_updates=*/true);
  MockAsyncPolicyLoader* raw_loader = loader.get();
  raw_loader->set_reload_interval(base::Milliseconds(100));

  EXPECT_CALL(*raw_loader, InitOnBackgroundThread());
  EXPECT_CALL(*raw_loader, Load()).Times(2);

  AsyncPolicyProvider provider(&schema_registry_, std::move(loader));
  provider.Init(&schema_registry_);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(HasSensitivePolicy(provider.policies()));

  raw_loader->SetPolicyBundleToLoad(CreateSensitivePolicyBundle());

  // Fast forward past the reload interval. The periodic reload queries
  // PlatformManagementService and calls Load().
  task_environment_.FastForwardBy(base::Milliseconds(150));
  EXPECT_TRUE(HasSensitivePolicy(provider.policies()));

  provider.Shutdown();
  task_environment_.RunUntilIdle();
}

}  // namespace policy
