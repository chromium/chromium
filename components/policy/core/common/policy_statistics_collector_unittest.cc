// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_statistics_collector.h"

#include <cstring>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/test/test_simple_task_runner.h"
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
const char* kTestPolicy3 = key::kExtensionInstallBlacklist;

const int kTestPolicy1Id = 42;
const int kTestPolicy2Id = 123;
const int kTestPolicy3Id = 32;

const char kTestChromeSchema[] =
    "{"
    "  \"type\": \"object\","
    "  \"properties\": {"
    "    \"Test Policy 1\": { \"type\": \"string\" },"
    "    \"Test Policy 2\": { \"type\": \"string\" },"
    "    \"ExtensionInstallBlacklist\": { \"type\": \"string\" },"
    "  }"
    "}";

const PolicyDetails kTestPolicyDetails[] = {
    // is_deprecated  is_device_policy              id  max_external_data_size
    {false, false, kTestPolicy1Id, 0},
    {false, false, kTestPolicy2Id, 0},
    {false, false, kTestPolicy3Id, 0},
};

class TestPolicyStatisticsCollector : public PolicyStatisticsCollector {
 public:
  TestPolicyStatisticsCollector(
      const GetChromePolicyDetailsCallback& get_details,
      const Schema& chrome_schema,
      PolicyService* policy_service,
      PrefService* prefs,
      const scoped_refptr<base::TaskRunner>& task_runner)
      : PolicyStatisticsCollector(get_details,
                                  chrome_schema,
                                  policy_service,
                                  prefs,
                                  task_runner) {}

  MOCK_METHOD1(RecordPolicyUse, void(int));
  MOCK_METHOD1(RecordPolicyIgnoredByAtomicGroup, void(int));
  MOCK_METHOD1(RecordPolicyGroupWithConflicts, void(int));
};

}  // namespace

class PolicyStatisticsCollectorTest : public testing::Test {
 protected:
  PolicyStatisticsCollectorTest()
      : update_delay_(base::TimeDelta::FromMilliseconds(
            PolicyStatisticsCollector::kStatisticsUpdateRate)),
        task_runner_(new base::TestSimpleTaskRunner()) {
  }

  void SetUp() override {
    std::string error;
    chrome_schema_ = Schema::Parse(kTestChromeSchema, &error);
    ASSERT_TRUE(chrome_schema_.valid()) << error;

    policy_details_.SetDetails(kTestPolicy1, &kTestPolicyDetails[0]);
    policy_details_.SetDetails(kTestPolicy2, &kTestPolicyDetails[1]);
    policy_details_.SetDetails(kTestPolicy3, &kTestPolicyDetails[2]);

    prefs_.registry()->RegisterInt64Pref(
        policy_prefs::kLastPolicyStatisticsUpdate, 0);

    // Set up default function behaviour.
    EXPECT_CALL(policy_service_,
                GetPolicies(PolicyNamespace(POLICY_DOMAIN_CHROME,
                                            std::string())))
        .WillRepeatedly(ReturnRef(policy_map_));

    // Arbitrary negative value (so it'll be different from |update_delay_|).
    last_delay_ = base::TimeDelta::FromDays(-1);
    policy_map_.Clear();
    policy_statistics_collector_.reset(new TestPolicyStatisticsCollector(
        policy_details_.GetCallback(),
        chrome_schema_,
        &policy_service_,
        &prefs_,
        task_runner_));
  }

  void SetPolicy(const std::string& name) {
    policy_map_.Set(name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                    POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(true),
                    nullptr);
  }

  void SetPolicyIgnoredByAtomicGroup(const std::string& name) {
    SetPolicy(name);
    auto* policy = policy_map_.GetMutable(name);
    policy->SetIgnoredByPolicyAtomicGroup();
  }

  base::TimeDelta GetFirstDelay() const {
    if (!task_runner_->HasPendingTask()) {
      ADD_FAILURE();
      return base::TimeDelta();
    }
    return task_runner_->NextPendingTaskDelay();
  }

  const base::TimeDelta update_delay_;

  base::TimeDelta last_delay_;

  PolicyDetailsMap policy_details_;
  Schema chrome_schema_;
  TestingPrefServiceSimple prefs_;
  MockPolicyService policy_service_;
  PolicyMap policy_map_;

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  std::unique_ptr<TestPolicyStatisticsCollector> policy_statistics_collector_;
};

TEST_F(PolicyStatisticsCollectorTest, CollectPending) {
  SetPolicy(kTestPolicy1);

  prefs_.SetInt64(policy_prefs::kLastPolicyStatisticsUpdate,
                  (base::Time::Now() - update_delay_).ToInternalValue());

  EXPECT_CALL(*policy_statistics_collector_, RecordPolicyUse(kTestPolicy1Id));

  policy_statistics_collector_->Initialize();
  EXPECT_EQ(1u, task_runner_->NumPendingTasks());
  EXPECT_EQ(update_delay_, GetFirstDelay());
}

TEST_F(PolicyStatisticsCollectorTest, CollectPendingVeryOld) {
  SetPolicy(kTestPolicy1);

  // Must not be 0.0 (read comment for Time::FromDoubleT).
  prefs_.SetInt64(policy_prefs::kLastPolicyStatisticsUpdate,
                  base::Time::FromDoubleT(1.0).ToInternalValue());

  EXPECT_CALL(*policy_statistics_collector_, RecordPolicyUse(kTestPolicy1Id));

  policy_statistics_collector_->Initialize();
  EXPECT_EQ(1u, task_runner_->NumPendingTasks());
  EXPECT_EQ(update_delay_, GetFirstDelay());
}

TEST_F(PolicyStatisticsCollectorTest, CollectLater) {
  SetPolicy(kTestPolicy1);

  prefs_.SetInt64(policy_prefs::kLastPolicyStatisticsUpdate,
                  (base::Time::Now() - update_delay_ / 2).ToInternalValue());

  policy_statistics_collector_->Initialize();
  EXPECT_EQ(1u, task_runner_->NumPendingTasks());
  EXPECT_LT(GetFirstDelay(), update_delay_);
}

TEST_F(PolicyStatisticsCollectorTest, MultiplePolicies) {
  SetPolicy(kTestPolicy1);
  SetPolicy(kTestPolicy2);

  prefs_.SetInt64(policy_prefs::kLastPolicyStatisticsUpdate,
                  (base::Time::Now() - update_delay_).ToInternalValue());

  EXPECT_CALL(*policy_statistics_collector_, RecordPolicyUse(kTestPolicy1Id));
  EXPECT_CALL(*policy_statistics_collector_, RecordPolicyUse(kTestPolicy2Id));

  policy_statistics_collector_->Initialize();
  EXPECT_EQ(1u, task_runner_->NumPendingTasks());
}

TEST_F(PolicyStatisticsCollectorTest, PolicyIgnoredByAtomicGroup) {
  SetPolicyIgnoredByAtomicGroup(kTestPolicy3);
  const AtomicGroup* extensions = nullptr;

  for (size_t i = 0; i < kPolicyAtomicGroupMappingsLength; ++i) {
    if (kPolicyAtomicGroupMappings[i].policy_group == group::kExtensions) {
      extensions = &kPolicyAtomicGroupMappings[i];
      break;
    }
  }

  DCHECK(extensions);

  prefs_.SetInt64(policy_prefs::kLastPolicyStatisticsUpdate,
                  (base::Time::Now() - update_delay_).ToInternalValue());

  EXPECT_CALL(*policy_statistics_collector_,
              RecordPolicyIgnoredByAtomicGroup(kTestPolicy3Id));
  EXPECT_CALL(*policy_statistics_collector_,
              RecordPolicyGroupWithConflicts(extensions->id));

  policy_statistics_collector_->Initialize();
  EXPECT_EQ(1u, task_runner_->NumPendingTasks());
}

}  // namespace policy
