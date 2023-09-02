// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/user_policy_metrics_recorder.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "testing/platform_test.h"

using UserPolicyMetricsRecorderTest = PlatformTest;

namespace policy {

// Tests that the user policy count metrics are correctly recorded.
TEST_F(UserPolicyMetricsRecorderTest, RecordCount) {
  base::HistogramTester histogram_tester;
  MockConfigurationPolicyProvider provider;
  UserPolicyMetricsRecorder recorder(&provider);

  // Do policy update.
  PolicyMap map;
  map.Set("test-policy", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
          POLICY_SOURCE_CLOUD, base::Value("hello"), nullptr);
  map.Set("test-policy-2", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
          POLICY_SOURCE_CLOUD, base::Value("hello2"), nullptr);
  provider.UpdateChromePolicy(map);

  const int expected_buckets_count = 1;
  const int expected_policy_count_bucket = 2;

  histogram_tester.ExpectBucketCount(kEnterpriseUserPolicyCountHistogram,
                                     expected_policy_count_bucket,
                                     expected_buckets_count);
}

// Tests that the user policy count metrics is only recorded once.
TEST_F(UserPolicyMetricsRecorderTest, RecordCount_DoneOnlyOnce) {
  base::HistogramTester histogram_tester;
  MockConfigurationPolicyProvider provider;
  UserPolicyMetricsRecorder recorder(&provider);

  // First update with a policy count of 1.
  {
    PolicyMap map;
    map.Set("test-policy", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
            POLICY_SOURCE_CLOUD, base::Value("hello"), nullptr);
    provider.UpdateChromePolicy(map);
  }

  // Second update with a policy count of 2.
  {
    PolicyMap map;
    map.Set("test-policy", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
            POLICY_SOURCE_CLOUD, base::Value("hello"), nullptr);
    map.Set("test-policy-2", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
            POLICY_SOURCE_CLOUD, base::Value("hello2"), nullptr);
    provider.UpdateChromePolicy(map);
  }

  const int expected_buckets_count = 1;
  const int expected_policy_count_bucket = 1;

  histogram_tester.ExpectBucketCount(kEnterpriseUserPolicyCountHistogram,
                                     expected_policy_count_bucket,
                                     expected_buckets_count);
}

}  // namespace policy
