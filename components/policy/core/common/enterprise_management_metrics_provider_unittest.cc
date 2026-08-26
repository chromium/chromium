// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/enterprise_management_metrics_provider.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/policy/core/common/enterprise_management_status_util.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace policy {

class MockManagementService : public ManagementService {
 public:
  MockManagementService() : ManagementService({}) {}
  ~MockManagementService() override = default;

  void SetAuthority(int authority) {
    SetManagementAuthoritiesForTesting(authority);
  }
};

class EnterpriseManagementMetricsProviderTest : public testing::Test {
 protected:
  EnterpriseManagementMetricsProviderTest() = default;
  ~EnterpriseManagementMetricsProviderTest() override = default;

  void SetUp() override {
    ON_CALL(policy_service_, GetPolicies)
        .WillByDefault(testing::ReturnRef(policy_map_));
  }

  std::unique_ptr<EnterpriseManagementMetricsProvider> CreateProvider(
      const std::vector<ManagementService*>& profile_services) {
    return std::make_unique<EnterpriseManagementMetricsProvider>(
        &platform_management_service_,
        base::BindRepeating(
            [](const std::vector<ManagementService*>& services,
               PolicyService* policy_service) {
              std::vector<EnterpriseManagementMetricsProvider::ProfileState>
                  states;
              for (ManagementService* service : services) {
                states.push_back({service, policy_service});
              }
              return states;
            },
            profile_services, &policy_service_));
  }

  void AddPolicies(int count) {
    for (int i = 0; i < count; ++i) {
      policy_map_.Set(base::StringPrintf("Policy%d", i), POLICY_LEVEL_MANDATORY,
                      POLICY_SCOPE_USER, POLICY_SOURCE_PLATFORM,
                      base::Value(true), nullptr);
    }
  }

  MockManagementService platform_management_service_;
  MockManagementService profile_management_service_1_;
  MockManagementService profile_management_service_2_;
  testing::NiceMock<MockPolicyService> policy_service_;
  PolicyMap policy_map_;
};

TEST_F(EnterpriseManagementMetricsProviderTest,
       RecordsPlatformAndProfilesCorrectly) {
  base::HistogramTester histogram_tester;

  // Setup platform status.
  platform_management_service_.SetAuthority(
      EnterpriseManagementAuthority::CLOUD);

  // Setup profile 1 status: Cloud Domain.
  profile_management_service_1_.SetAuthority(
      EnterpriseManagementAuthority::CLOUD_DOMAIN);

  // Setup profile 2 status: Computer Local with <= 3 policies.
  profile_management_service_2_.SetAuthority(
      EnterpriseManagementAuthority::COMPUTER_LOCAL);

  AddPolicies(2);

  auto provider = CreateProvider(
      {&profile_management_service_1_, &profile_management_service_2_});

  provider->ProvideCurrentSessionData(nullptr);

  // Platform: Expect 1 sample of kCloud (3)
  histogram_tester.ExpectUniqueSample(
      "Enterprise.ManagementService.PlatformManagementStatus",
      static_cast<int>(PlatformManagementStatus::kCloud), 1);

  // Profiles: Expect 1 sample of kCloudDomain (5) and 1 sample of
  // kComputerLocalLe3 (1)
  histogram_tester.ExpectBucketCount(
      "Enterprise.ManagementService.BrowserManagementStatus",
      static_cast<int>(BrowserManagementStatus::kCloudDomain), 1);
  histogram_tester.ExpectBucketCount(
      "Enterprise.ManagementService.BrowserManagementStatus",
      static_cast<int>(BrowserManagementStatus::kComputerLocalLe3), 1);
  histogram_tester.ExpectTotalCount(
      "Enterprise.ManagementService.BrowserManagementStatus", 2);
}

TEST_F(EnterpriseManagementMetricsProviderTest,
       RecordsMultipleAuthoritiesForPlatform) {
  base::HistogramTester histogram_tester;

  // Setup platform status to have BOTH Cloud and Computer Local.
  platform_management_service_.SetAuthority(
      EnterpriseManagementAuthority::CLOUD |
      EnterpriseManagementAuthority::COMPUTER_LOCAL);

  auto provider = CreateProvider({});

  provider->ProvideCurrentSessionData(nullptr);

  // Platform: Expect kCloud (3) and kComputerLocal (1)
  histogram_tester.ExpectBucketCount(
      "Enterprise.ManagementService.PlatformManagementStatus",
      static_cast<int>(PlatformManagementStatus::kCloud), 1);
  histogram_tester.ExpectBucketCount(
      "Enterprise.ManagementService.PlatformManagementStatus",
      static_cast<int>(PlatformManagementStatus::kComputerLocal), 1);
  histogram_tester.ExpectTotalCount(
      "Enterprise.ManagementService.PlatformManagementStatus", 2);
}

TEST_F(EnterpriseManagementMetricsProviderTest,
       RecordsProfileComputerLocalGt3) {
  base::HistogramTester histogram_tester;

  // Setup profile status: Computer Local with > 3 policies.
  profile_management_service_1_.SetAuthority(
      EnterpriseManagementAuthority::COMPUTER_LOCAL);

  AddPolicies(4);

  auto provider = CreateProvider({&profile_management_service_1_});

  provider->ProvideCurrentSessionData(nullptr);

  // Profiles: Expect 1 sample of kComputerLocalGt3 (2)
  histogram_tester.ExpectUniqueSample(
      "Enterprise.ManagementService.BrowserManagementStatus",
      static_cast<int>(BrowserManagementStatus::kComputerLocalGt3), 1);
}

TEST_F(EnterpriseManagementMetricsProviderTest,
       ProvideSystemProfileMetrics_RecordsPlatformAndProfilesCorrectly) {
  // Setup platform status.
  platform_management_service_.SetAuthority(
      EnterpriseManagementAuthority::CLOUD);

  // Setup profile 1 status: Cloud Domain.
  profile_management_service_1_.SetAuthority(
      EnterpriseManagementAuthority::CLOUD_DOMAIN);

  // Setup profile 2 status: Computer Local with <= 3 policies.
  profile_management_service_2_.SetAuthority(
      EnterpriseManagementAuthority::COMPUTER_LOCAL);

  AddPolicies(2);

  auto provider = CreateProvider(
      {&profile_management_service_1_, &profile_management_service_2_});

  metrics::SystemProfileProto system_profile;
  provider->ProvideSystemProfileMetrics(&system_profile);

  const auto& enterprise = system_profile.enterprise_management();
  EXPECT_THAT(
      enterprise.platform_status(),
      testing::UnorderedElementsAre(
          metrics::SystemProfileProto::EnterpriseManagement::PLATFORM_CLOUD));

  EXPECT_THAT(enterprise.browser_status(),
              testing::UnorderedElementsAre(
                  metrics::SystemProfileProto::EnterpriseManagement::
                      BROWSER_COMPUTER_LOCAL_LE3,
                  metrics::SystemProfileProto::EnterpriseManagement::
                      BROWSER_CLOUD_DOMAIN));
}

TEST_F(EnterpriseManagementMetricsProviderTest,
       ProvideSystemProfileMetrics_DeduplicatesBrowserStatuses) {
  // Setup profile 1 & 2 status: Cloud Domain.
  profile_management_service_1_.SetAuthority(
      EnterpriseManagementAuthority::CLOUD_DOMAIN);
  profile_management_service_2_.SetAuthority(
      EnterpriseManagementAuthority::CLOUD_DOMAIN);

  auto provider = CreateProvider(
      {&profile_management_service_1_, &profile_management_service_2_});

  metrics::SystemProfileProto system_profile;
  provider->ProvideSystemProfileMetrics(&system_profile);

  const auto& enterprise = system_profile.enterprise_management();
  EXPECT_THAT(enterprise.browser_status(),
              testing::UnorderedElementsAre(
                  metrics::SystemProfileProto::EnterpriseManagement::
                      BROWSER_CLOUD_DOMAIN));
}

}  // namespace policy
