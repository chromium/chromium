// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/enterprise_management_status_util.h"

#include "base/values.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_map.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace policy {

class EnterpriseManagementStatusUtilTest : public testing::Test {
 protected:
  EnterpriseManagementStatusUtilTest()
      : platform_management_service_({}), browser_management_service_({}) {}

  void SetUp() override {
    ON_CALL(policy_service_, GetPolicies)
        .WillByDefault(testing::ReturnRef(policy_map_));
  }

  void AddPolicies(size_t count) {
    const size_t start = policy_map_.size();
    for (size_t i = 0; i < count; ++i) {
      policy_map_.Set(absl::StrFormat("Policy%zu", start + i),
                      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                      POLICY_SOURCE_PLATFORM, base::Value(true), nullptr);
    }
  }

  ManagementService platform_management_service_;
  ManagementService browser_management_service_;
  testing::NiceMock<MockPolicyService> policy_service_;
  PolicyMap policy_map_;
};

TEST_F(EnterpriseManagementStatusUtilTest, NullServicesAreUnmanaged) {
  base::DictValue status = GetManagementServiceStatus(
      /*platform_management_service=*/nullptr,
      /*browser_management_service=*/nullptr, /*policy_service=*/nullptr);

  EXPECT_THAT(*status.FindList(kPlatformManagementStatusKey),
              testing::ElementsAre("Unmanaged"));
  EXPECT_THAT(*status.FindList(kBrowserManagementStatusKey),
              testing::ElementsAre("Unmanaged"));
}

TEST_F(EnterpriseManagementStatusUtilTest, MapsAuthoritiesToStrings) {
  {
    base::DictValue status = GetManagementServiceStatus(
        &platform_management_service_, &browser_management_service_,
        &policy_service_);
    EXPECT_THAT(*status.FindList(kPlatformManagementStatusKey),
                testing::ElementsAre("Unmanaged"));
    EXPECT_THAT(*status.FindList(kBrowserManagementStatusKey),
                testing::ElementsAre("Unmanaged"));
  }

  platform_management_service_.SetManagementAuthoritiesForTesting(
      EnterpriseManagementAuthority::DOMAIN_LOCAL |
      EnterpriseManagementAuthority::COMPUTER_LOCAL);
  browser_management_service_.SetManagementAuthoritiesForTesting(
      EnterpriseManagementAuthority::CLOUD_DOMAIN |
      EnterpriseManagementAuthority::CLOUD);

  base::DictValue status = GetManagementServiceStatus(
      &platform_management_service_, &browser_management_service_,
      &policy_service_);
  EXPECT_THAT(*status.FindList(kPlatformManagementStatusKey),
              testing::ElementsAre("Domain Local", "Computer Local"));
  EXPECT_THAT(*status.FindList(kBrowserManagementStatusKey),
              testing::ElementsAre("Cloud Domain", "Cloud"));
}

TEST_F(EnterpriseManagementStatusUtilTest,
       BrowserComputerLocalBucketsByPolicyCount) {
  browser_management_service_.SetManagementAuthoritiesForTesting(
      EnterpriseManagementAuthority::COMPUTER_LOCAL);

  AddPolicies(3);
  EXPECT_THAT(*GetManagementServiceStatus(&platform_management_service_,
                                          &browser_management_service_,
                                          &policy_service_)
                   .FindList(kBrowserManagementStatusKey),
              testing::ElementsAre("Computer Local (<= 3 policies)"));

  AddPolicies(1);
  EXPECT_THAT(*GetManagementServiceStatus(&platform_management_service_,
                                          &browser_management_service_,
                                          &policy_service_)
                   .FindList(kBrowserManagementStatusKey),
              testing::ElementsAre("Computer Local (> 3 policies)"));
}

}  // namespace policy
