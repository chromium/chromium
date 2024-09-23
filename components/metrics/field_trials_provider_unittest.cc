// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/metrics/field_trials_provider.h"

#include <string_view>

#include "base/metrics/field_trial.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/platform_thread.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/synthetic_trial_registry.h"
#include "components/variations/synthetic_trials.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/system_profile.pb.h"

using ActiveGroup = base::FieldTrial::ActiveGroup;

namespace variations {

namespace {

constexpr const char* kSuffix = "UKM";

const ActiveGroup kFieldTrials[] = {{"Trial1", "Group1"},
                                    {"Trial2", "Group2"},
                                    {"Trial3", "Group3"}};
const ActiveGroup kSyntheticFieldTrials[] = {{"Synthetic1", "SyntheticGroup1"},
                                             {"Synthetic2", "SyntheticGroup2"}};

ActiveGroupId ToActiveGroupId(ActiveGroup active_group,
                              std::string suffix = "");

const ActiveGroupId kFieldTrialIds[] = {ToActiveGroupId(kFieldTrials[0]),
                                        ToActiveGroupId(kFieldTrials[1]),
                                        ToActiveGroupId(kFieldTrials[2])};
const ActiveGroupId kAllTrialIds[] = {
    ToActiveGroupId(kFieldTrials[0]), ToActiveGroupId(kFieldTrials[1]),
    ToActiveGroupId(kFieldTrials[2]), ToActiveGroupId(kSyntheticFieldTrials[0]),
    ToActiveGroupId(kSyntheticFieldTrials[1])};
const ActiveGroupId kAllTrialIdsWithSuffixes[] = {
    ToActiveGroupId(kFieldTrials[0], kSuffix),
    ToActiveGroupId(kFieldTrials[1], kSuffix),
    ToActiveGroupId(kFieldTrials[2], kSuffix),
    ToActiveGroupId(kSyntheticFieldTrials[0], kSuffix),
    ToActiveGroupId(kSyntheticFieldTrials[1], kSuffix)};

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

ActiveGroupId ToActiveGroupId(ActiveGroup active_group, std::string suffix) {
  return MakeActiveGroupId(active_group.trial_name + suffix,
                           active_group.group_name + suffix);
}

}  // namespace

class FieldTrialsProviderTest : public ::testing::Test {
 public:
  FieldTrialsProviderTest() { scope_.InitWithEmptyFeatureAndFieldTrialLists(); }

  ~FieldTrialsProviderTest() override = default;

 protected:
  void SetUp() override {
    // Register the field trials.
    for (const ActiveGroup& trial : kFieldTrials) {
      base::FieldTrial* field_trial = base::FieldTrialList::CreateFieldTrial(
          trial.trial_name, trial.group_name);
      // Call Activate() to finalize and mark the field trial as active.
      field_trial->Activate();
    }
  }

  // Register trials which should get recorded.
  void RegisterExpectedSyntheticTrials() {
    for (const ActiveGroup& trial : kSyntheticFieldTrials) {
      registry_.RegisterSyntheticFieldTrial(SyntheticTrialGroup(
          trial.trial_name, trial.group_name,
          /*annotation_mode=*/
          variations::SyntheticTrialAnnotationMode::kNextLog));
    }
  }
  // Register trial which shouldn't get recorded.
  void RegisterExtraSyntheticTrial() {
    registry_.RegisterSyntheticFieldTrial(SyntheticTrialGroup(
        "ExtraSynthetic", "ExtraGroup",
        /*annotation_mode=*/
        variations::SyntheticTrialAnnotationMode::kNextLog));
  }

  // Waits until base::TimeTicks::Now() no longer equals |value|. This should
  // take between 1-15ms per the documented resolution of base::TimeTicks.
  void WaitUntilTimeChanges(const base::TimeTicks& value) {
    while (base::TimeTicks::Now() == value) {
      base::PlatformThread::Sleep(base::Milliseconds(1));
    }
  }

  SyntheticTrialRegistry registry_;
  base::test::ScopedFeatureList scope_;
};

TEST_F(FieldTrialsProviderTest, ProvideSyntheticTrials) {
  FieldTrialsProvider provider(&registry_, std::string_view());

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
  FieldTrialsProvider provider(nullptr, std::string_view());

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

  FieldTrialsProvider provider(&registry_, std::string_view());
  RegisterExpectedSyntheticTrials();
  WaitUntilTimeChanges(base::TimeTicks::Now());
  provider.SetLogCreationTimeForTesting(base::TimeTicks::Now());

  provider.ProvideCurrentSessionData(&uma_log);

  EXPECT_EQ(std::size(kAllTrialIds),
            static_cast<size_t>(uma_log.system_profile().field_trial_size()));
  CheckFieldTrialsInSystemProfile(uma_log.system_profile(), kAllTrialIds);
}

TEST_F(FieldTrialsProviderTest, GetAndWriteFieldTrialsWithSuffixes) {
  metrics::ChromeUserMetricsExtension uma_log;
  uma_log.system_profile();

  FieldTrialsProvider provider(&registry_, kSuffix);
  RegisterExpectedSyntheticTrials();
  WaitUntilTimeChanges(base::TimeTicks::Now());
  provider.SetLogCreationTimeForTesting(base::TimeTicks::Now());

  provider.ProvideCurrentSessionData(&uma_log);

  EXPECT_EQ(std::size(kAllTrialIdsWithSuffixes),
            static_cast<size_t>(uma_log.system_profile().field_trial_size()));
  CheckFieldTrialsInSystemProfile(uma_log.system_profile(),
                                  kAllTrialIdsWithSuffixes);
}

}  // namespace variations
