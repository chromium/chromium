// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/management/management_service.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/prefs/persistent_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

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

}  // namespace policy
