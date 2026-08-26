// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/management/management_service.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/policy/core/common/features.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/prefs/persistent_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/win_util.h"
#include "components/policy/core/common/management/platform_management_service.h"
#include "components/policy/core/common/management/platform_management_status_provider_win.h"
#endif

namespace policy {

constexpr char kPrefName[] = "pref_name";

class TestManagementStatusProvider : public ManagementStatusProvider {
 public:
  explicit TestManagementStatusProvider(const std::string& cache_pref_name,
                                        EnterpriseManagementAuthority authority)
      : ManagementStatusProvider(cache_pref_name), authority_(authority) {}
  ~TestManagementStatusProvider() override = default;

 protected:
  // Returns the authority responsible for the management.
  EnterpriseManagementAuthority FetchAuthority() override { return authority_; }

 private:
  EnterpriseManagementAuthority authority_;
};

class TestManagementService : public ManagementService {
 public:
  TestManagementService() : ManagementService({}) {}
  explicit TestManagementService(
      std::vector<std::unique_ptr<ManagementStatusProvider>> providers)
      : ManagementService(std::move(providers)) {}
  void SetManagementStatusProviderForTesting(
      std::vector<std::unique_ptr<ManagementStatusProvider>> providers) {
    SetManagementStatusProvider(std::move(providers));
  }
};

class ManagementServiceTests : public testing::Test {
 public:
  ManagementServiceTests(const ManagementServiceTests&) = delete;
  ManagementServiceTests& operator=(const ManagementServiceTests&) = delete;

  void SetUp() override {
    prefs_.registry()->RegisterIntegerPref(kPrefName, 0);
    ManagementService::RegisterLocalStatePrefs(prefs_.registry());
  }

  PrefService* prefs() { return &prefs_; }
  scoped_refptr<TestingPrefStore> user_prefs_store() {
    return prefs_.user_prefs_store();
  }

 protected:
  ManagementServiceTests() = default;
  ~ManagementServiceTests() override = default;

 private:
  TestingPrefServiceSimple prefs_;
};

TEST_F(ManagementServiceTests, ScopedManagementServiceOverrideForTesting) {
  TestManagementService management_service;
  EXPECT_FALSE(management_service.IsManaged());
  EXPECT_FALSE(management_service.HasManagementAuthority(
      EnterpriseManagementAuthority::COMPUTER_LOCAL));
  EXPECT_FALSE(management_service.HasManagementAuthority(
      EnterpriseManagementAuthority::DOMAIN_LOCAL));
  EXPECT_FALSE(management_service.HasManagementAuthority(
      EnterpriseManagementAuthority::CLOUD));
  EXPECT_EQ(ManagementAuthorityTrustworthiness::NONE,
            management_service.GetManagementAuthorityTrustworthiness());

  {
    ScopedManagementServiceOverrideForTesting override_1(
        &management_service, EnterpriseManagementAuthority::CLOUD_DOMAIN);
    EXPECT_TRUE(management_service.IsManaged());
    EXPECT_TRUE(management_service.HasManagementAuthority(
        EnterpriseManagementAuthority::CLOUD_DOMAIN));
    EXPECT_EQ(ManagementAuthorityTrustworthiness::FULLY_TRUSTED,
              management_service.GetManagementAuthorityTrustworthiness());
    {
      ScopedManagementServiceOverrideForTesting override_2(
          &management_service, EnterpriseManagementAuthority::CLOUD);
      EXPECT_TRUE(management_service.IsManaged());
      EXPECT_TRUE(management_service.HasManagementAuthority(
          EnterpriseManagementAuthority::CLOUD));
      EXPECT_EQ(ManagementAuthorityTrustworthiness::TRUSTED,
                management_service.GetManagementAuthorityTrustworthiness());
    }
    EXPECT_TRUE(management_service.IsManaged());
    EXPECT_TRUE(management_service.HasManagementAuthority(
        EnterpriseManagementAuthority::CLOUD_DOMAIN));
    EXPECT_EQ(ManagementAuthorityTrustworthiness::FULLY_TRUSTED,
              management_service.GetManagementAuthorityTrustworthiness());
  }
  EXPECT_FALSE(management_service.IsManaged());
  EXPECT_FALSE(management_service.HasManagementAuthority(
      EnterpriseManagementAuthority::COMPUTER_LOCAL));
  EXPECT_FALSE(management_service.HasManagementAuthority(
      EnterpriseManagementAuthority::DOMAIN_LOCAL));
  EXPECT_FALSE(management_service.HasManagementAuthority(
      EnterpriseManagementAuthority::CLOUD));
  EXPECT_EQ(ManagementAuthorityTrustworthiness::NONE,
            management_service.GetManagementAuthorityTrustworthiness());
}

TEST_F(ManagementServiceTests, LoadCachedValues) {
  base::test::TaskEnvironment task_environment;
  prefs()->SetInteger(kPrefName, EnterpriseManagementAuthority::CLOUD);

  std::vector<std::unique_ptr<ManagementStatusProvider>> providers;
  providers.emplace_back(std::make_unique<TestManagementStatusProvider>(
      kPrefName, EnterpriseManagementAuthority::CLOUD_DOMAIN));
  providers.emplace_back(std::make_unique<TestManagementStatusProvider>(
      std::string(), EnterpriseManagementAuthority::COMPUTER_LOCAL));

  TestManagementService management_service(std::move(providers));
  management_service.UsePrefStoreAsCache(user_prefs_store());

  EXPECT_TRUE(management_service.HasManagementAuthority(
      EnterpriseManagementAuthority::CLOUD));
  EXPECT_TRUE(management_service.HasManagementAuthority(
      EnterpriseManagementAuthority::COMPUTER_LOCAL));
  EXPECT_FALSE(management_service.HasManagementAuthority(
      EnterpriseManagementAuthority::CLOUD_DOMAIN));
  EXPECT_FALSE(management_service.HasManagementAuthority(
      EnterpriseManagementAuthority::DOMAIN_LOCAL));
  EXPECT_EQ(management_service.GetManagementAuthorityTrustworthiness(),
            ManagementAuthorityTrustworthiness::TRUSTED);

  management_service.UsePrefServiceAsCache(prefs());

  base::test::TestFuture<ManagementAuthorityTrustworthiness,
                         ManagementAuthorityTrustworthiness>
      test_future;
  management_service.RefreshCache(test_future.GetCallback());
  EXPECT_EQ(test_future.Get<0>(), ManagementAuthorityTrustworthiness::TRUSTED);
  EXPECT_EQ(test_future.Get<1>(),
            ManagementAuthorityTrustworthiness::FULLY_TRUSTED);

  EXPECT_FALSE(management_service.HasManagementAuthority(
      EnterpriseManagementAuthority::CLOUD));
  EXPECT_TRUE(management_service.HasManagementAuthority(
      EnterpriseManagementAuthority::COMPUTER_LOCAL));
  EXPECT_TRUE(management_service.HasManagementAuthority(
      EnterpriseManagementAuthority::CLOUD_DOMAIN));
  EXPECT_FALSE(management_service.HasManagementAuthority(
      EnterpriseManagementAuthority::DOMAIN_LOCAL));
  EXPECT_EQ(management_service.GetManagementAuthorityTrustworthiness(),
            ManagementAuthorityTrustworthiness::FULLY_TRUSTED);
}

class AsyncTestManagementStatusProvider : public ManagementStatusProvider {
 public:
  explicit AsyncTestManagementStatusProvider(const std::string& cache_pref_name,
                                             EnterpriseManagementAuthority authority)
      : ManagementStatusProvider(cache_pref_name), authority_(authority) {}
  ~AsyncTestManagementStatusProvider() override = default;

  void FetchAuthorityAsync(
      base::OnceCallback<void(std::pair<ManagementStatusProvider*,
                                        EnterpriseManagementAuthority>)>
          callback) override {
    callback_ = std::move(callback);
  }

  void RunCallback() {
    if (callback_) {
      std::move(callback_).Run({this, authority_});
    }
  }

 protected:
  EnterpriseManagementAuthority FetchAuthority() override { return authority_; }

 private:
  EnterpriseManagementAuthority authority_;
  base::OnceCallback<void(std::pair<ManagementStatusProvider*,
                                    EnterpriseManagementAuthority>)>
      callback_;
};

TEST_F(ManagementServiceTests, AsyncRefreshCache) {
  base::test::TaskEnvironment task_environment;
  prefs()->SetInteger(kPrefName, EnterpriseManagementAuthority::NONE);

  auto provider1 = std::make_unique<AsyncTestManagementStatusProvider>(
      kPrefName, EnterpriseManagementAuthority::CLOUD);
  auto* provider1_ptr = provider1.get();

  std::vector<std::unique_ptr<ManagementStatusProvider>> providers;
  providers.push_back(std::move(provider1));

  auto management_service =
      std::make_unique<TestManagementService>(std::move(providers));
  management_service->UsePrefServiceAsCache(prefs());

  base::test::TestFuture<ManagementAuthorityTrustworthiness,
                         ManagementAuthorityTrustworthiness>
      test_future;
  management_service->RefreshCache(test_future.GetCallback());

  EXPECT_EQ(management_service->GetManagementAuthorityTrustworthiness(),
            ManagementAuthorityTrustworthiness::NONE);

  provider1_ptr->RunCallback();

  EXPECT_EQ(test_future.Get<0>(), ManagementAuthorityTrustworthiness::NONE);
  EXPECT_EQ(test_future.Get<1>(), ManagementAuthorityTrustworthiness::TRUSTED);
  EXPECT_EQ(management_service->GetManagementAuthorityTrustworthiness(),
            ManagementAuthorityTrustworthiness::TRUSTED);
}

TEST_F(ManagementServiceTests, RefreshCacheWeakPtrSafety) {
  base::test::TaskEnvironment task_environment;
  prefs()->SetInteger(kPrefName, EnterpriseManagementAuthority::NONE);

  auto provider1 = std::make_unique<AsyncTestManagementStatusProvider>(
      kPrefName, EnterpriseManagementAuthority::CLOUD);

  std::vector<std::unique_ptr<ManagementStatusProvider>> providers;
  providers.push_back(std::move(provider1));

  auto management_service =
      std::make_unique<TestManagementService>(std::move(providers));
  management_service->UsePrefServiceAsCache(prefs());

  base::test::TestFuture<ManagementAuthorityTrustworthiness,
                         ManagementAuthorityTrustworthiness>
      test_future;
  management_service->RefreshCache(test_future.GetCallback());

  management_service.reset();
}

#if BUILDFLAG(IS_WIN)
class AzureActiveDirectoryManagementServiceTests
    : public ManagementServiceTests,
      public testing::WithParamInterface<
          base::win::ScopedAzureADJoinStateForTesting::AzureADJoinType> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    AzureActiveDirectoryManagementServiceTests,
    testing::Values(
        base::win::ScopedAzureADJoinStateForTesting::AzureADJoinType::
            kWorkplace,
        base::win::ScopedAzureADJoinStateForTesting::AzureADJoinType::kDevice));

TEST_P(AzureActiveDirectoryManagementServiceTests,
       AzureActiveDirectoryProviders) {
  base::test::TaskEnvironment task_environment;
  base::win::ScopedAzureADJoinStateForTesting scoped_azure_ad_join_state(
      GetParam());

  // AzureActiveDirectoryStatusProvider returns CLOUD_DOMAIN for both workplace
  // and device join.
  {
    std::vector<std::unique_ptr<ManagementStatusProvider>> providers;
    providers.push_back(std::make_unique<AzureActiveDirectoryStatusProvider>());
    TestManagementService management_service(std::move(providers));
    management_service.UsePrefServiceAsCache(prefs());

    base::test::TestFuture<ManagementAuthorityTrustworthiness,
                           ManagementAuthorityTrustworthiness>
        test_future;
    management_service.RefreshCache(test_future.GetCallback());
    EXPECT_EQ(test_future.Get<1>(),
              ManagementAuthorityTrustworthiness::FULLY_TRUSTED);
  }

  // AzureActiveDirectoryDeviceStatusProvider returns CLOUD_DOMAIN for device
  // join and NONE for workplace join.
  {
    std::vector<std::unique_ptr<ManagementStatusProvider>> providers;
    providers.push_back(
        std::make_unique<AzureActiveDirectoryDeviceStatusProvider>());
    TestManagementService management_service(std::move(providers));
    management_service.UsePrefServiceAsCache(prefs());

    base::test::TestFuture<ManagementAuthorityTrustworthiness,
                           ManagementAuthorityTrustworthiness>
        test_future;
    management_service.RefreshCache(test_future.GetCallback());
    ManagementAuthorityTrustworthiness expected_trustworthiness =
        GetParam() == base::win::ScopedAzureADJoinStateForTesting::
                          AzureADJoinType::kDevice
            ? ManagementAuthorityTrustworthiness::FULLY_TRUSTED
            : ManagementAuthorityTrustworthiness::NONE;
    EXPECT_EQ(test_future.Get<1>(), expected_trustworthiness);
  }
}

// TODO(crbug.com/531448879): Revert this change when AzureAD logic migration is
// complete.
TEST_P(AzureActiveDirectoryManagementServiceTests,
       PlatformManagementServicePolicyLoadingTrust) {
  base::test::TaskEnvironment task_environment;
  // Ensure actual host AD domain join state does not affect test results, so
  // this test works on developer machines. If join state is true,
  // GetManagementAuthorityTrustworthinessForPolicyLoading will be TRUSTED,
  // which breaks the kWorkplace case, which expects it to be NONE.
  base::win::ScopedDomainStateForTesting scoped_domain_state(false);
  base::win::ScopedAzureADJoinStateForTesting scoped_azure_ad_join_state(
      GetParam());
  base::win::ScopedDeviceRegisteredWithManagementForTesting
      scoped_device_registered(false);

  PlatformManagementService* service = PlatformManagementService::GetInstance();
  service->UsePrefServiceAsCache(prefs());

  base::test::TestFuture<ManagementAuthorityTrustworthiness,
                         ManagementAuthorityTrustworthiness>
      test_future;
  service->RefreshCache(test_future.GetCallback());
  ASSERT_TRUE(test_future.Wait());

  // General trustworthiness includes AzureActiveDirectoryStatusProvider
  // (CLOUD_DOMAIN).
  EXPECT_EQ(service->GetManagementAuthorityTrustworthiness(),
            ManagementAuthorityTrustworthiness::FULLY_TRUSTED);

  // Policy loading trustworthiness excludes AzureActiveDirectoryStatusProvider,
  // checking only AzureActiveDirectoryDeviceStatusProvider (FULLY_TRUSTED for
  // device join, NONE for workplace join).
  ManagementAuthorityTrustworthiness expected_trustworthiness =
      GetParam() == base::win::ScopedAzureADJoinStateForTesting::
                        AzureADJoinType::kDevice
          ? ManagementAuthorityTrustworthiness::FULLY_TRUSTED
          : ManagementAuthorityTrustworthiness::NONE;
  EXPECT_EQ(service->GetManagementAuthorityTrustworthinessForPolicyLoading(),
            expected_trustworthiness);

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndDisableFeature(
        features::kFilterSensitivePoliciesOnWorkplaceJoinedDevices);

    base::test::TestFuture<ManagementAuthorityTrustworthiness,
                           ManagementAuthorityTrustworthiness>
        disabled_future;
    service->RefreshCache(disabled_future.GetCallback());
    ASSERT_TRUE(disabled_future.Wait());

    EXPECT_EQ(service->GetManagementAuthorityTrustworthinessForPolicyLoading(),
              ManagementAuthorityTrustworthiness::FULLY_TRUSTED);
  }
}
#endif

}  // namespace policy
