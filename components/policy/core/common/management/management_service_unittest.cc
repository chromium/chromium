// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class TestManagementStatusProvider : public ManagementStatusProvider {
 public:
  TestManagementStatusProvider(EnterpriseManagementAuthority authority,
                               bool managed)
      : authority_(authority), managed_(managed) {}
  ~TestManagementStatusProvider() override = default;

  // Returns |true| if the service or component is managed.
  bool IsManaged() override { return managed_; }

  // Returns the authority responsible for the management.
  EnterpriseManagementAuthority GetAuthority() override { return authority_; }

 private:
  EnterpriseManagementAuthority authority_;
  bool managed_;
};

class TestManagementService : public ManagementService {
 public:
  TestManagementService()
      : ManagementService(ManagementTarget::kMaxValue, {}) {}
  explicit TestManagementService(
      ManagementTarget target,
      std::vector<std::unique_ptr<ManagementStatusProvider>> providers)
      : ManagementService(target, std::move(providers)) {}
  void SetManagementStatusProviderForTesting(
      std::vector<std::unique_ptr<ManagementStatusProvider>> providers) {
    SetManagementStatusProvider(std::move(providers));
  }
};

TEST(ManagementService, ScopedManagementServiceOverrideForTesting) {
  TestManagementService platform_management_service(ManagementTarget::PLATFORM,
                                                    {});
  TestManagementService browser_management_service(ManagementTarget::BROWSER,
                                                   {});
  std::vector<std::unique_ptr<ManagementStatusProvider>> providers;
  providers.emplace_back(std::make_unique<TestManagementStatusProvider>(
      EnterpriseManagementAuthority::CLOUD, true));
  providers.emplace_back(std::make_unique<TestManagementStatusProvider>(
      EnterpriseManagementAuthority::CLOUD_DOMAIN, true));
  providers.emplace_back(std::make_unique<TestManagementStatusProvider>(
      EnterpriseManagementAuthority::COMPUTER_LOCAL, true));
  providers.emplace_back(std::make_unique<TestManagementStatusProvider>(
      EnterpriseManagementAuthority::DOMAIN_LOCAL, true));
  platform_management_service.SetManagementStatusProviderForTesting(
      std::move(providers));
  auto platform_authorities =
      platform_management_service.GetManagementAuthorities();
  EXPECT_TRUE(browser_management_service.GetManagementAuthorities().empty());
  EXPECT_EQ(platform_authorities.size(), 4u);
  EXPECT_NE(platform_authorities.find(EnterpriseManagementAuthority::CLOUD),
            platform_authorities.end());
  EXPECT_NE(
      platform_authorities.find(EnterpriseManagementAuthority::CLOUD_DOMAIN),
      platform_authorities.end());
  EXPECT_NE(
      platform_authorities.find(EnterpriseManagementAuthority::COMPUTER_LOCAL),
      platform_authorities.end());
  EXPECT_NE(
      platform_authorities.find(EnterpriseManagementAuthority::DOMAIN_LOCAL),
      platform_authorities.end());
  {
    ScopedManagementServiceOverrideForTesting
        scoped_management_service_override(
            ManagementTarget::PLATFORM,
            base::flat_set<EnterpriseManagementAuthority>(
                {EnterpriseManagementAuthority::CLOUD,
                 EnterpriseManagementAuthority::DOMAIN_LOCAL}));
    auto platform_authorities =
        platform_management_service.GetManagementAuthorities();
    EXPECT_TRUE(browser_management_service.GetManagementAuthorities().empty());
    EXPECT_EQ(platform_authorities.size(), 2u);
    EXPECT_NE(platform_authorities.find(EnterpriseManagementAuthority::CLOUD),
              platform_authorities.end());
    EXPECT_NE(
        platform_authorities.find(EnterpriseManagementAuthority::DOMAIN_LOCAL),
        platform_authorities.end());
  }
  {
    ScopedManagementServiceOverrideForTesting
        scoped_management_service_override(
            ManagementTarget::PLATFORM,
            base::flat_set<EnterpriseManagementAuthority>());
    EXPECT_TRUE(browser_management_service.GetManagementAuthorities().empty());
    EXPECT_TRUE(platform_management_service.GetManagementAuthorities().empty());
  }
}

// Tests that only the authorities that are actively managing are returned.
TEST(ManagementService, GetManagementAuthorities) {
  TestManagementService management_service;
  auto authorities = management_service.GetManagementAuthorities();
  EXPECT_TRUE(authorities.empty());
  std::vector<std::unique_ptr<ManagementStatusProvider>> providers;
  providers.emplace_back(std::make_unique<TestManagementStatusProvider>(
      EnterpriseManagementAuthority::CLOUD, true));
  providers.emplace_back(std::make_unique<TestManagementStatusProvider>(
      EnterpriseManagementAuthority::CLOUD_DOMAIN, false));
  providers.emplace_back(std::make_unique<TestManagementStatusProvider>(
      EnterpriseManagementAuthority::COMPUTER_LOCAL, false));
  providers.emplace_back(std::make_unique<TestManagementStatusProvider>(
      EnterpriseManagementAuthority::DOMAIN_LOCAL, true));
  management_service.SetManagementStatusProviderForTesting(
      std::move(providers));
  authorities = management_service.GetManagementAuthorities();
  EXPECT_EQ(authorities.size(), 2u);
  EXPECT_NE(authorities.find(EnterpriseManagementAuthority::CLOUD),
            authorities.end());
  EXPECT_NE(authorities.find(EnterpriseManagementAuthority::DOMAIN_LOCAL),
            authorities.end());
  ManagementAuthorityTrustworthiness trustworthyness =
      management_service.GetManagementAuthorityTrustworthiness();
  EXPECT_EQ(trustworthyness, ManagementAuthorityTrustworthiness::TRUSTED);
}

}  // namespace policy
