// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class TestManagementStatusProvider : public ManagementStatusProvider {
 public:
  explicit TestManagementStatusProvider(EnterpriseManagementAuthority authority)
      : authority_(authority) {}
  ~TestManagementStatusProvider() override = default;

  // Returns the authority responsible for the management.
  EnterpriseManagementAuthority GetAuthority() override { return authority_; }

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

TEST(ManagementService, ScopedManagementServiceOverrideForTesting) {
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

}  // namespace policy
