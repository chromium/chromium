// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/child_process_field_trial_syncer.h"

#include <string>
#include <vector>

#include "base/metrics/field_trial.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

TEST(ChildProcessFieldTrialSyncerTest, FieldTrialState) {
  base::test::TaskEnvironment task_environment;
  base::test::ScopedFeatureList scoped_feature_list;

  // We expect there are no field trials, because we creates 3 field trials
  // A, B and C, and checks EXPECT_EQ("*A/G1/B/G2/C/G3/", states_string).
  // So we need to create a new scope with empty feature and field trial lists.
  scoped_feature_list.InitWithEmptyFeatureAndFieldTrialLists();

  base::FieldTrial* trial1 = base::FieldTrialList::CreateFieldTrial("A", "G1");
  base::FieldTrial* trial2 = base::FieldTrialList::CreateFieldTrial("B", "G2");
  base::FieldTrial* trial3 = base::FieldTrialList::CreateFieldTrial("C", "G3");
  // Activate trial3 before command line is produced.
  trial1->Activate();

  std::string states_string;
  base::FieldTrialList::AllStatesToString(&states_string);
  EXPECT_EQ("*A/G1/B/G2/C/G3", states_string);

  // Active trial 2 before creating the syncer.
  trial2->Activate();

  std::vector<std::string> observed_trial_names;
  auto callback =
      base::BindLambdaForTesting([&](const std::string& trial_name) {
        observed_trial_names.push_back(trial_name);
      });

  std::set<std::string> initially_active_trials = {"A"};
  ChildProcessFieldTrialSyncer::CreateInstanceForTesting(
      initially_active_trials, callback);

  // The callback should be invoked for activated trials that were not initially
  // active, but were activated later. In this case, trial 2. (Trial 1 was
  // already active and so its state shouldn't be notified.)
  EXPECT_THAT(observed_trial_names, testing::ElementsAre("B"));

  // Now, activate trial 3, which should also get reflected.
  trial3->Activate();
  EXPECT_THAT(observed_trial_names, testing::ElementsAre("B", "C"));

  ChildProcessFieldTrialSyncer::DeleteInstanceForTesting();
}

}  // namespace variations
