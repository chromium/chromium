// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/field_trials_provider.h"

#include "base/threading/platform_thread.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/synthetic_trial_registry.h"
#include "components/variations/synthetic_trials.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace variations {

namespace {

const ActiveGroupId kFieldTrialIds[] = {{37, 43}, {13, 47}, {23, 17}};
const ActiveGroupId kSyntheticTrialIds[] = {{55, 15}, {66, 16}};
const ActiveGroupId kAllTrialIds[] = {{37, 43},
                                      {13, 47},
                                      {23, 17},
                                      {55, 15},
                                      {66, 16}};

class TestProvider : public FieldTrialsProvider {
 public:
  TestProvider(SyntheticTrialRegistry* registry, base::StringPiece suffix)
      : FieldTrialsProvider(registry, suffix) {}
  ~TestProvider() override {}

  void GetFieldTrialIds(
      std::vector<ActiveGroupId>* field_trial_ids) const override {
    ASSERT_TRUE(field_trial_ids->empty());
    for (const ActiveGroupId& id : kFieldTrialIds) {
      field_trial_ids->push_back(id);
    }
  }
};

// Check that the field trials in |system_profile| correspond to |expected|.
void CheckFieldTrialsInSystemProfile(
    const metrics::SystemProfileProto& system_profile,
    const ActiveGroupId* expected) {
  for (int i = 0; i < system_profile.field_trial_size(); ++i) {
    const metrics::SystemProfileProto::FieldTrial& field_trial =
        system_profile.field_trial(i);
    EXPECT_EQ(expected[i].name, field_trial.name_id());
    EXPECT_EQ(expected[i].group, field_trial.group_id());
  }
}

}  // namespace

class FieldTrialsProviderTest : public ::testing::Test {
 public:
  FieldTrialsProviderTest() {}
  ~FieldTrialsProviderTest() override {}

 protected:
  // Register trials which should get recorded.
  void RegisterExpectedSyntheticTrials() {
    for (const ActiveGroupId& id : kSyntheticTrialIds) {
      registry_.RegisterSyntheticFieldTrial(SyntheticTrialGroup(
          id.name, id.group,
          variations::SyntheticTrialAnnotationMode::kNextLog));
    }
  }
  // Register trial which shouldn't get recorded.
  void RegisterExtraSyntheticTrial() {
    registry_.RegisterSyntheticFieldTrial(SyntheticTrialGroup(
        100, 1000, variations::SyntheticTrialAnnotationMode::kNextLog));
  }

  // Waits until base::TimeTicks::Now() no longer equals |value|. This should
  // take between 1-15ms per the documented resolution of base::TimeTicks.
  void WaitUntilTimeChanges(const base::TimeTicks& value) {
    while (base::TimeTicks::Now() == value) {
      base::PlatformThread::Sleep(base::Milliseconds(1));
    }
  }

  SyntheticTrialRegistry registry_;
};

TEST_F(FieldTrialsProviderTest, ProvideSyntheticTrials) {
  TestProvider provider(&registry_, base::StringPiece());

  RegisterExpectedSyntheticTrials();
  // Make sure these trials are older than the log.
  WaitUntilTimeChanges(base::TimeTicks::Now());

  // Get the current time and wait for it to change.
  base::TimeTicks log_creation_time = base::TimeTicks::Now();

  // Make sure that the log is older than the trials that should be excluded.
  WaitUntilTimeChanges(log_creation_time);

  RegisterExtraSyntheticTrial();

  metrics::SystemProfileProto proto;
  provider.ProvideSystemProfileMetricsWithLogCreationTime(log_creation_time,
                                                          &proto);

  EXPECT_EQ(std::size(kAllTrialIds),
            static_cast<size_t>(proto.field_trial_size()));
  CheckFieldTrialsInSystemProfile(proto, kAllTrialIds);
}

TEST_F(FieldTrialsProviderTest, NoSyntheticTrials) {
  TestProvider provider(nullptr, base::StringPiece());

  metrics::SystemProfileProto proto;
  provider.ProvideSystemProfileMetricsWithLogCreationTime(base::TimeTicks(),
                                                          &proto);

  EXPECT_EQ(std::size(kFieldTrialIds),
            static_cast<size_t>(proto.field_trial_size()));
  CheckFieldTrialsInSystemProfile(proto, kFieldTrialIds);
}

TEST_F(FieldTrialsProviderTest, ProvideCurrentSessionData) {
  metrics::ChromeUserMetricsExtension uma_log;
  uma_log.system_profile();

  // {1, 1} should not be in the resulting proto as ProvideCurrentSessionData()
  // clears existing trials and sets the trials to be those determined by
  // GetSyntheticFieldTrialsOlderThan() and GetFieldTrialIds().
  metrics::SystemProfileProto::FieldTrial* trial =
      uma_log.mutable_system_profile()->add_field_trial();
  trial->set_name_id(1);
  trial->set_group_id(1);

  TestProvider provider(&registry_, base::StringPiece());
  RegisterExpectedSyntheticTrials();
  WaitUntilTimeChanges(base::TimeTicks::Now());
  provider.SetLogCreationTimeForTesting(base::TimeTicks::Now());

  provider.ProvideCurrentSessionData(&uma_log);

  EXPECT_EQ(std::size(kAllTrialIds),
            static_cast<size_t>(uma_log.system_profile().field_trial_size()));
  CheckFieldTrialsInSystemProfile(uma_log.system_profile(), kAllTrialIds);
}

}  // namespace variations
