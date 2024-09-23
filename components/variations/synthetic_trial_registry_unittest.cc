// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/synthetic_trial_registry.h"

#include <string>

#include "base/containers/contains.h"
#include "base/metrics/field_trial.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/hashing.h"
#include "components/variations/synthetic_trials.h"
#include "components/variations/synthetic_trials_active_group_id_provider.h"
#include "components/variations/variations_crash_keys.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {
namespace {

using ::testing::_;
using ::testing::Contains;

class MockSyntheticTrialObserver : public SyntheticTrialObserver {
 public:
  MockSyntheticTrialObserver() = default;
  ~MockSyntheticTrialObserver() override = default;

  MOCK_METHOD(void,
              OnSyntheticTrialsChanged,
              (const std::vector<SyntheticTrialGroup>&,
               const std::vector<SyntheticTrialGroup>&,
               const std::vector<SyntheticTrialGroup>&),
              (override));
};

}  // namespace

bool operator==(const SyntheticTrialGroup& a, const SyntheticTrialGroup& b) {
  return a.group_name() == b.group_name() && a.trial_name() == b.trial_name() &&
         a.annotation_mode() == b.annotation_mode();
}

class SyntheticTrialRegistryTest : public ::testing::Test {
 public:
  SyntheticTrialRegistryTest() { InitCrashKeys(); }

  SyntheticTrialRegistryTest(const SyntheticTrialRegistryTest&) = delete;
  SyntheticTrialRegistryTest& operator=(const SyntheticTrialRegistryTest&) =
      delete;

  ~SyntheticTrialRegistryTest() override { ClearCrashKeysInstanceForTesting(); }

  // Returns true if there is a synthetic trial in the given vector that matches
  // the given trial name and trial group; returns false otherwise.
  bool HasSyntheticTrial(const std::vector<ActiveGroupId>& synthetic_trials,
                         const std::string& trial_name,
                         const std::string& trial_group) {
    uint32_t trial_name_hash = HashName(trial_name);
    uint32_t trial_group_hash = HashName(trial_group);
    for (const ActiveGroupId& trial : synthetic_trials) {
      if (trial.name == trial_name_hash && trial.group == trial_group_hash)
        return true;
    }
    return false;
  }

  // Waits until base::TimeTicks::Now() no longer equals |value|. This should
  // take between 1-15ms per the documented resolution of base::TimeTicks.
  void WaitUntilTimeChanges(const base::TimeTicks& value) {
    while (base::TimeTicks::Now() == value) {
      base::PlatformThread::Sleep(base::Milliseconds(1));
    }
  }

  // Gets the current synthetic trials.
  void GetSyntheticTrials(const SyntheticTrialRegistry& registry,
                          std::vector<ActiveGroupId>* synthetic_trials) {
    // Ensure that time has advanced by at least a tick before proceeding.
    WaitUntilTimeChanges(base::TimeTicks::Now());
    registry.GetSyntheticFieldTrialsOlderThan(base::TimeTicks::Now(),
                                              synthetic_trials);
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(SyntheticTrialRegistryTest, RegisterSyntheticTrial) {
  SyntheticTrialRegistry registry;

  // Add two synthetic trials and confirm that they show up in the list.
  SyntheticTrialGroup trial1_group1("TestTrial1", "Group1",
                                    SyntheticTrialAnnotationMode::kNextLog);
  registry.RegisterSyntheticFieldTrial(trial1_group1);

  SyntheticTrialGroup trial2_group2("TestTrial2", "Group2",
                                    SyntheticTrialAnnotationMode::kNextLog);
  registry.RegisterSyntheticFieldTrial(trial2_group2);
  // Ensure that time has advanced by at least a tick before proceeding.
  WaitUntilTimeChanges(base::TimeTicks::Now());

  // Save the time when the log was started (it's okay for this to be greater
  // than the time recorded by the above call since it's used to ensure the
  // value changes).
  const base::TimeTicks begin_log_time = base::TimeTicks::Now();

  std::vector<ActiveGroupId> synthetic_trials;
  registry.GetSyntheticFieldTrialsOlderThan(base::TimeTicks::Now(),
                                            &synthetic_trials);
  EXPECT_EQ(2U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial1", "Group1"));
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial2", "Group2"));

  // Ensure that time has advanced by at least a tick before proceeding.
  WaitUntilTimeChanges(begin_log_time);

  // Change the group for the first trial after the log started.
  SyntheticTrialGroup trial1_group2("TestTrial1", "Group2",
                                    SyntheticTrialAnnotationMode::kNextLog);
  registry.RegisterSyntheticFieldTrial(trial1_group2);
  registry.GetSyntheticFieldTrialsOlderThan(begin_log_time, &synthetic_trials);
  EXPECT_EQ(1U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial2", "Group2"));

  // Add a new trial after the log started and confirm that it doesn't show up.
  SyntheticTrialGroup trial3_group3("TestTrial3", "Group3",
                                    SyntheticTrialAnnotationMode::kNextLog);
  registry.RegisterSyntheticFieldTrial(trial3_group3);
  registry.GetSyntheticFieldTrialsOlderThan(begin_log_time, &synthetic_trials);
  EXPECT_EQ(1U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial2", "Group2"));

  // Change TestTrial3 to be active immediately, add a new trial that also is
  // active immediately, and confirm they both show up despite being added after
  // the log started.
  SyntheticTrialGroup trial3_group3_current_log(
      "TestTrial3", "Group3", SyntheticTrialAnnotationMode::kCurrentLog);
  SyntheticTrialGroup trial4_group4_current_log(
      "TestTrial4", "Group4", SyntheticTrialAnnotationMode::kCurrentLog);
  registry.RegisterSyntheticFieldTrial(trial3_group3_current_log);
  registry.RegisterSyntheticFieldTrial(trial4_group4_current_log);
  registry.GetSyntheticFieldTrialsOlderThan(begin_log_time, &synthetic_trials);
  ASSERT_EQ(3U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial2", "Group2"));
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial3", "Group3"));
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial4", "Group4"));

  // Start a new log and ensure all four trials appear in it.
  GetSyntheticTrials(registry, &synthetic_trials);
  ASSERT_EQ(4U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial1", "Group2"));
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial2", "Group2"));
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial3", "Group3"));
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial4", "Group4"));
}

TEST_F(SyntheticTrialRegistryTest, GetSyntheticFieldTrialsOlderThanSuffix) {
  SyntheticTrialRegistry registry;
  SyntheticTrialGroup trial("TestTrial", "Group",
                            SyntheticTrialAnnotationMode::kCurrentLog);
  registry.RegisterSyntheticFieldTrial(trial);

  std::vector<ActiveGroupId> synthetic_trials;
  // Get list of synthetic trials, but with no added suffixes to the trial and
  // group names.
  registry.GetSyntheticFieldTrialsOlderThan(base::TimeTicks::Now(),
                                            &synthetic_trials);
  ASSERT_EQ(1U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial", "Group"));

  // Get list of synthetic trials, but with "UKM" suffixed to the trial and
  // group names.
  registry.GetSyntheticFieldTrialsOlderThan(base::TimeTicks::Now(),
                                            &synthetic_trials, "UKM");
  ASSERT_EQ(1U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrialUKM", "GroupUKM"));
}

TEST_F(SyntheticTrialRegistryTest, RegisterExternalExperiments_NoAllowlist) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      internal::kExternalExperimentAllowlist);
  SyntheticTrialRegistry registry;
  const auto mode = SyntheticTrialRegistry::kOverrideExistingIds;

  registry.RegisterExternalExperiments({100, 200}, mode);
  std::vector<ActiveGroupId> synthetic_trials;
  GetSyntheticTrials(registry, &synthetic_trials);
  EXPECT_EQ(0U, synthetic_trials.size());

  registry.RegisterExternalExperiments({500}, mode);
  GetSyntheticTrials(registry, &synthetic_trials);
  EXPECT_EQ(0U, synthetic_trials.size());
}

TEST_F(SyntheticTrialRegistryTest, RegisterExternalExperiments_WithAllowlist) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      internal::kExternalExperimentAllowlist,
      {{"100", "A"}, {"101", "A"}, {"300", "C,xyz"}});

  const auto override_mode = SyntheticTrialRegistry::kOverrideExistingIds;
  SyntheticTrialRegistry registry;
  std::vector<ActiveGroupId> synthetic_trials;

  // Register a synthetic trial TestTrial1 with groups A and B.
  registry.RegisterExternalExperiments({100, 200, 300}, override_mode);
  GetSyntheticTrials(registry, &synthetic_trials);
  EXPECT_EQ(2U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "A", "100"));
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "C", "300"));

  // A new call that only contains 100 will clear the other ones.
  registry.RegisterExternalExperiments({101}, override_mode);
  GetSyntheticTrials(registry, &synthetic_trials);
  EXPECT_EQ(1U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "A", "101"));

  const auto dont_override = SyntheticTrialRegistry::kDoNotOverrideExistingIds;
  // Now, register another id that doesn't exist with kDoNotOverrideExistingIds.
  registry.RegisterExternalExperiments({300}, dont_override);
  GetSyntheticTrials(registry, &synthetic_trials);
  EXPECT_EQ(2U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "A", "101"));
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "C", "300"));

  // Registering 100, which already has a trial A registered, shouldn't work.
  registry.RegisterExternalExperiments({100}, dont_override);
  GetSyntheticTrials(registry, &synthetic_trials);
  EXPECT_EQ(2U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "A", "101"));
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "C", "300"));

  // Registering an empty set should also do nothing.
  registry.RegisterExternalExperiments({}, dont_override);
  GetSyntheticTrials(registry, &synthetic_trials);
  EXPECT_EQ(2U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "A", "101"));
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "C", "300"));

  // Registering with an override should reset existing ones.
  registry.RegisterExternalExperiments({100}, override_mode);
  GetSyntheticTrials(registry, &synthetic_trials);
  EXPECT_EQ(1U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "A", "100"));
}

TEST_F(SyntheticTrialRegistryTest, GetSyntheticFieldTrialActiveGroups) {
  SyntheticTrialRegistry registry;

  // Instantiate and set up the corresponding singleton observer which tracks
  // the creation of all SyntheticTrialGroups.
  registry.AddObserver(SyntheticTrialsActiveGroupIdProvider::GetInstance());

  // Add two synthetic trials and confirm that they show up in the list.
  SyntheticTrialGroup trial1("TestTrial1", "Group1",
                             SyntheticTrialAnnotationMode::kNextLog);
  registry.RegisterSyntheticFieldTrial(trial1);

  SyntheticTrialGroup trial2("TestTrial2", "Group2",
                             SyntheticTrialAnnotationMode::kNextLog);
  registry.RegisterSyntheticFieldTrial(trial2);

  // Ensure that time has advanced by at least a tick before proceeding.
  WaitUntilTimeChanges(base::TimeTicks::Now());

  // Now get the list of currently active groups.
  std::vector<std::string> output;
  GetSyntheticTrialGroupIdsAsString(&output);
  EXPECT_EQ(2U, output.size());

  ActiveGroupId trial1_id = trial1.id();
  std::string trial1_hash =
      base::StringPrintf("%x-%x", trial1_id.name, trial1_id.group);
  EXPECT_TRUE(base::Contains(output, trial1_hash));

  ActiveGroupId trial2_id = trial2.id();
  std::string trial2_hash =
      base::StringPrintf("%x-%x", trial2_id.name, trial2.id().group);
  EXPECT_TRUE(base::Contains(output, trial2_hash));
}

TEST_F(SyntheticTrialRegistryTest, NotifyObserver) {
  SyntheticTrialRegistry registry;
  MockSyntheticTrialObserver observer;
  registry.AddObserver(&observer);

  // A brand new synthetic trial is registered. Observers should be notified.
  SyntheticTrialGroup trial1("TestTrial1", "Group1",
                             SyntheticTrialAnnotationMode::kNextLog);
  EXPECT_CALL(observer, OnSyntheticTrialsChanged(
                            std::vector<SyntheticTrialGroup>({trial1}),
                            std::vector<SyntheticTrialGroup>(),
                            std::vector<SyntheticTrialGroup>({trial1})));
  registry.RegisterSyntheticFieldTrial(trial1);

  // A brand new synthetic trial is registered. Observers should be notified.
  SyntheticTrialGroup trial2("TestTrial2", "Group1",
                             SyntheticTrialAnnotationMode::kNextLog);
  EXPECT_CALL(observer,
              OnSyntheticTrialsChanged(
                  std::vector<SyntheticTrialGroup>({trial2}),
                  std::vector<SyntheticTrialGroup>(),
                  std::vector<SyntheticTrialGroup>({trial1, trial2})));
  registry.RegisterSyntheticFieldTrial(trial2);

  // The group of a trial has changed. Observers should be notified.
  SyntheticTrialGroup trial3("TestTrial1", "Group2",
                             SyntheticTrialAnnotationMode::kNextLog);
  EXPECT_CALL(observer,
              OnSyntheticTrialsChanged(
                  std::vector<SyntheticTrialGroup>({trial3}),
                  std::vector<SyntheticTrialGroup>(),
                  std::vector<SyntheticTrialGroup>({trial3, trial2})));
  registry.RegisterSyntheticFieldTrial(trial3);

  // The annotation mode of a trial has changed. Observers should be notified.
  SyntheticTrialGroup trial4("TestTrial1", "Group2",
                             SyntheticTrialAnnotationMode::kCurrentLog);
  EXPECT_CALL(observer,
              OnSyntheticTrialsChanged(
                  std::vector<SyntheticTrialGroup>({trial4}),
                  std::vector<SyntheticTrialGroup>(),
                  std::vector<SyntheticTrialGroup>({trial4, trial2})));
  registry.RegisterSyntheticFieldTrial(trial4);

  // A synthetic trial is re-registered with no changes. Observers should NOT be
  // notified.
  EXPECT_CALL(observer, OnSyntheticTrialsChanged(_, _, _)).Times(0);
  registry.RegisterSyntheticFieldTrial(trial4);

  registry.RemoveObserver(&observer);
}

TEST_F(SyntheticTrialRegistryTest, NotifyObserverExternalTrials) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      internal::kExternalExperimentAllowlist,
      {{"100", "A"}, {"101", "B"}, {"102", "C"}});
  SyntheticTrialRegistry registry;
  MockSyntheticTrialObserver observer;
  registry.AddObserver(&observer);

  const auto mode = SyntheticTrialRegistry::kOverrideExistingIds;
  const SyntheticTrialGroup kTrial1("A", "100",
                                    SyntheticTrialAnnotationMode::kNextLog);
  const SyntheticTrialGroup kTrial2("B", "101",
                                    SyntheticTrialAnnotationMode::kNextLog);
  const SyntheticTrialGroup kTrial3("C", "102",
                                    SyntheticTrialAnnotationMode::kNextLog);
  // A trial that's not in the allowlist.
  const SyntheticTrialGroup kTrial4("D", "103",
                                    SyntheticTrialAnnotationMode::kNextLog);

  EXPECT_CALL(observer,
              OnSyntheticTrialsChanged(
                  std::vector<SyntheticTrialGroup>({kTrial1, kTrial2}),
                  std::vector<SyntheticTrialGroup>(),
                  std::vector<SyntheticTrialGroup>({kTrial1, kTrial2})));
  registry.RegisterExternalExperiments({100, 101}, mode);

  // Registering one trial with override should remove existing trials.
  EXPECT_CALL(observer,
              OnSyntheticTrialsChanged(
                  std::vector<SyntheticTrialGroup>({kTrial3}),
                  std::vector<SyntheticTrialGroup>({kTrial1, kTrial2}),
                  std::vector<SyntheticTrialGroup>({kTrial3})));
  registry.RegisterExternalExperiments({102}, mode);

  registry.RemoveObserver(&observer);
}

}  // namespace variations
