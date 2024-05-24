// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_statistics_collector.h"

#include <cstring>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_test_utils.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

using testing::ReturnRef;

// Arbitrary policy names used for testing.
const char kTestPolicy1[] = "Test Policy 1";
const char kTestPolicy2[] = "Test Policy 2";
const char* kTestPolicy3 = key::kDefaultSearchProviderEncodings;

const char kEnrollmentTokenPolicy[] = "CloudManagementEnrollmentToken";
const char kEnrollmentOptionPolicy[] = "CloudManagementEnrollmentMandatory";
const char kBrowserSigninPolicy[] = "BrowserSignin";

const int kTestPolicy1Id = 42;
const int kTestPolicy2Id = 123;
const int kTestPolicy3Id = 32;
const int kEnrollmentTokenPolicyId = 510;
const int kEnrollmentOptionPolicyId = 505;
const int kBrowserSigninPolicyId = 487;

const char kTestChromeSchema[] = R"(
    {
      "type": "object",
      "properties": {
        "Test Policy 1": { "type": "boolean" },
        "Test Policy 2": { "type": "boolean" },
        "DefaultSearchProviderEncodings": { "type": "boolean" },
        "CloudManagementEnrollmentToken": { "type": "boolean" },
        "CloudManagementEnrollmentMandatory": { "type": "boolean" },
        "BrowserSigninPolicy": { "type": "integer" },
      }
    })";

const PolicyDetails kTestPolicyDetails[] = {
    // is_deprecated is_future is_device_policy id  max_external_data_size
    {false, false, kProfile, kTestPolicy1Id, 0},
    {false, false, kProfile, kTestPolicy2Id, 0},
    {false, false, kProfile, kTestPolicy3Id, 0},
    {false, false, kProfile, kEnrollmentTokenPolicyId, 0},
    {false, false, kProfile, kEnrollmentOptionPolicyId, 0},
    {false, false, kProfile, kBrowserSigninPolicyId, 0},
};

}  // namespace

class PolicyStatisticsCollectorTest : public testing::Test {
 protected:
  PolicyStatisticsCollectorTest()
      : task_runner_(new base::TestSimpleTaskRunner()) {}

  void SetUp() override {
    ASSIGN_OR_RETURN(chrome_schema_, Schema::Parse(kTestChromeSchema),
                     [](const auto& e) { ADD_FAILURE() << e; });

    policy_details_.SetDetails(kTestPolicy1, &kTestPolicyDetails[0]);
    policy_details_.SetDetails(kTestPolicy2, &kTestPolicyDetails[1]);
    policy_details_.SetDetails(kTestPolicy3, &kTestPolicyDetails[2]);
    policy_details_.SetDetails(kEnrollmentTokenPolicy, &kTestPolicyDetails[3]);
    policy_details_.SetDetails(kEnrollmentOptionPolicy, &kTestPolicyDetails[4]);
    policy_details_.SetDetails(kBrowserSigninPolicy, &kTestPolicyDetails[5]);

    prefs_.registry()->RegisterInt64Pref(
        policy_prefs::kLastPolicyStatisticsUpdate, 0);

    // Set up default function behaviour.
    EXPECT_CALL(policy_service_, GetPolicies(PolicyNamespace(
                                     POLICY_DOMAIN_CHROME, std::string())))
        .WillRepeatedly(ReturnRef(policy_map_));

    policy_map_.Clear();
    policy_statistics_collector_ = std::make_unique<PolicyStatisticsCollector>(
        policy_details_.GetCallback(), chrome_schema_, &policy_service_,
        &prefs_, task_runner_);
  }

  void SetPolicy(const std::string& name,
                 PolicyLevel level = POLICY_LEVEL_MANDATORY) {
    policy_map_.Set(name, level, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                    base::Value(true), /*external_data_fetcher=*/nullptr);
  }

  void SetPolicy(const std::string& name, PolicySource source) {
    policy_map_.Set(name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, source,
                    base::Value(true), /*external_data_fetcher=*/nullptr);
  }

  void SetPolicyIgnoredByAtomicGroup(const std::string& name) {
    SetPolicy(name, POLICY_LEVEL_MANDATORY);
    auto* policy = policy_map_.GetMutable(name);
    policy->SetIgnoredByPolicyAtomicGroup();
  }

  void SetBrowserSigninPolicy(const int& policyValue) {
    policy_map_.Set(kBrowserSigninPolicy, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                    base::Value(policyValue),
                    /*external_data_fetcher=*/nullptr);
  }

  base::TimeDelta GetFirstDelay() const {
    if (!task_runner_->HasPendingTask()) {
      ADD_FAILURE();
      return base::TimeDelta();
    }
    return task_runner_->NextPendingTaskDelay();
  }

  PolicyDetailsMap policy_details_;
  Schema chrome_schema_;
  TestingPrefServiceSimple prefs_;
  MockPolicyService policy_service_;
  PolicyMap policy_map_;

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  std::unique_ptr<PolicyStatisticsCollector> policy_statistics_collector_;

  base::HistogramTester histogram_tester_;
};

TEST_F(PolicyStatisticsCollectorTest, NoPolicy) {
  policy_statistics_collector_->Initialize();

  histogram_tester_.ExpectTotalCount("Enterprise.Policies.Mandatory", 0);
  histogram_tester_.ExpectTotalCount("Enterprise.Policies.Recommended", 0);
  histogram_tester_.ExpectTotalCount("Enterprise.Policies", 0);
  histogram_tester_.ExpectTotalCount("Enterprise.Policies.Sources", 0);
  histogram_tester_.ExpectTotalCount("Enterprise.BrowserSigninPolicy", 0);
}

TEST_F(PolicyStatisticsCollectorTest, CollectPending) {
  SetPolicy(kTestPolicy1, POLICY_LEVEL_MANDATORY);

  policy_statistics_collector_->Initialize();

  histogram_tester_.ExpectBucketCount("Enterprise.Policies", kTestPolicy1Id, 1);

  EXPECT_EQ(1u, task_runner_->NumPendingTasks());
}

TEST_F(PolicyStatisticsCollectorTest, CollectPendingVeryOld) {
  SetPolicy(kTestPolicy1, POLICY_LEVEL_MANDATORY);

  // Must not be 0.0 (read comment for Time::FromSecondsSinceUnixEpoch).
  prefs_.SetTime(policy_prefs::kLastPolicyStatisticsUpdate,
                 base::Time::FromSecondsSinceUnixEpoch(1.0));

  policy_statistics_collector_->Initialize();

  histogram_tester_.ExpectBucketCount("Enterprise.Policies", kTestPolicy1Id, 1);

  EXPECT_EQ(1u, task_runner_->NumPendingTasks());
}

TEST_F(PolicyStatisticsCollectorTest, CollectLater) {
  SetPolicy(kTestPolicy1, POLICY_LEVEL_MANDATORY);

  prefs_.SetTime(
      policy_prefs::kLastPolicyStatisticsUpdate,
      base::Time::Now() - PolicyStatisticsCollector::kStatisticsUpdateRate / 2);

  policy_statistics_collector_->Initialize();

  histogram_tester_.ExpectTotalCount("Enterprise.Policies", 0);

  EXPECT_EQ(1u, task_runner_->NumPendingTasks());
}

TEST_F(PolicyStatisticsCollectorTest, MultiplePolicies) {
  SetPolicy(kTestPolicy1, POLICY_LEVEL_MANDATORY);
  SetPolicy(kTestPolicy2, POLICY_LEVEL_RECOMMENDED);

  policy_statistics_collector_->Initialize();

  histogram_tester_.ExpectBucketCount("Enterprise.Policies", kTestPolicy1Id, 1);
  histogram_tester_.ExpectBucketCount("Enterprise.Policies", kTestPolicy2Id, 1);
  histogram_tester_.ExpectTotalCount("Enterprise.Policies", 2);
}

TEST_F(PolicyStatisticsCollectorTest, MandatoryPolicy) {
  SetPolicy(kTestPolicy1, POLICY_LEVEL_MANDATORY);

  policy_statistics_collector_->Initialize();

  histogram_tester_.ExpectUniqueSample("Enterprise.Policies.Mandatory",
                                       kTestPolicy1Id, 1);
  histogram_tester_.ExpectTotalCount("Enterprise.Policies.Recommended", 0);
}

TEST_F(PolicyStatisticsCollectorTest, RecommendedPolicy) {
  SetPolicy(kTestPolicy2, POLICY_LEVEL_RECOMMENDED);

  policy_statistics_collector_->Initialize();

  histogram_tester_.ExpectUniqueSample("Enterprise.Policies.Recommended",
                                       kTestPolicy2Id, 1);
  histogram_tester_.ExpectTotalCount("Enterprise.Policies.Mandatory", 0);
}

TEST_F(PolicyStatisticsCollectorTest, CloudOnly) {
  SetPolicy(kTestPolicy1, POLICY_SOURCE_CLOUD);
  SetPolicy(kTestPolicy2, POLICY_SOURCE_CLOUD_FROM_ASH);

  policy_statistics_collector_->Initialize();

  histogram_tester_.ExpectUniqueSample("Enterprise.Policies.Sources",
                                       PoliciesSources::kCloudOnly, 1);
}

TEST_F(PolicyStatisticsCollectorTest, PlatformOnly) {
  SetPolicy(kTestPolicy1, POLICY_SOURCE_PLATFORM);
  SetPolicy(kTestPolicy2, POLICY_SOURCE_ACTIVE_DIRECTORY);
  SetPolicy(kEnrollmentTokenPolicy, POLICY_SOURCE_PLATFORM);

  policy_statistics_collector_->Initialize();

  histogram_tester_.ExpectUniqueSample("Enterprise.Policies.Sources",
                                       PoliciesSources::kPlatformOnly, 1);
}

TEST_F(PolicyStatisticsCollectorTest, Hybrid) {
  SetPolicy(kTestPolicy1, POLICY_SOURCE_PLATFORM);
  SetPolicy(kTestPolicy2, POLICY_SOURCE_CLOUD);

  policy_statistics_collector_->Initialize();

  histogram_tester_.ExpectUniqueSample("Enterprise.Policies.Sources",
                                       PoliciesSources::kHybrid, 1);
}

TEST_F(PolicyStatisticsCollectorTest, CloudExcepptEnrollment) {
  SetPolicy(kTestPolicy1, POLICY_SOURCE_CLOUD);
  SetPolicy(kEnrollmentTokenPolicy, POLICY_SOURCE_PLATFORM);
  SetPolicy(kEnrollmentOptionPolicy, POLICY_SOURCE_PLATFORM);

  policy_statistics_collector_->Initialize();

  histogram_tester_.ExpectUniqueSample(
      "Enterprise.Policies.Sources",
      PoliciesSources::kCloudOnlyExceptEnrollment, 1);
}

TEST_F(PolicyStatisticsCollectorTest, EnrollmentOnly) {
  SetPolicy(kEnrollmentTokenPolicy, POLICY_SOURCE_PLATFORM);
  SetPolicy(kEnrollmentOptionPolicy, POLICY_SOURCE_PLATFORM);

  policy_statistics_collector_->Initialize();

  histogram_tester_.ExpectUniqueSample("Enterprise.Policies.Sources",
                                       PoliciesSources::kEnrollmentOnly, 1);
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(PolicyStatisticsCollectorTest, BrowserSigninValid) {
  SetBrowserSigninPolicy(static_cast<int>(BrowserSigninMode::kDisabled));

  policy_statistics_collector_->Initialize();

  histogram_tester_.ExpectUniqueSample("Enterprise.BrowserSigninPolicy",
                                       BrowserSigninMode::kDisabled, 1);
}

TEST_F(PolicyStatisticsCollectorTest, BrowserSigninInValid) {
  // 3 is an invalid value for BrowserSigninMode
  SetBrowserSigninPolicy(3);

  policy_statistics_collector_->Initialize();

  histogram_tester_.ExpectTotalCount("Enterprise.BrowserSigninPolicy", 0);
}
#endif

}  // namespace policy
