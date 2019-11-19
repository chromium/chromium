// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/child_process_field_trial_syncer.h"

#include <string>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/variations/variations_crash_keys.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

namespace {

// TestFieldTrialObserver to listen to be notified by the child process syncer.
class TestFieldTrialObserver : public base::FieldTrialList::Observer {
 public:
  TestFieldTrialObserver() {}
  ~TestFieldTrialObserver() override { ClearCrashKeysInstanceForTesting(); }

  // base::FieldTrial::Observer:
  void OnFieldTrialGroupFinalized(const std::string& trial_name,
                                  const std::string& group_name) override {
    observed_entries_.push_back(std::make_pair(trial_name, group_name));
  }

  size_t observed_entries_count() const { return observed_entries_.size(); }

  std::pair<std::string, std::string> get_observed_entry(int i) const {
    return observed_entries_[i];
  }

 private:
  std::vector<std::pair<std::string, std::string>> observed_entries_;

  DISALLOW_COPY_AND_ASSIGN(TestFieldTrialObserver);
};

// Needed because make_pair("a", "b") doesn't convert to std::string pair.
std::pair<std::string, std::string> MakeStringPair(const std::string& a,
                                                   const std::string& b) {
  return std::make_pair(a, b);
}

}  // namespace

TEST(ChildProcessFieldTrialSyncerTest, FieldTrialState) {
  base::test::SingleThreadTaskEnvironment task_environment;

  // We don't use the descriptor here anyways so it's ok to pass -1.
  base::FieldTrialList::CreateTrialsFromCommandLine(
      *base::CommandLine::ForCurrentProcess(), "field_trial_handle_switch", -1);

  base::FieldTrial* trial1 = base::FieldTrialList::CreateFieldTrial("A", "G1");
  base::FieldTrial* trial2 = base::FieldTrialList::CreateFieldTrial("B", "G2");
  base::FieldTrial* trial3 = base::FieldTrialList::CreateFieldTrial("C", "G3");
  // Activate trial3 before command line is produced.
  trial1->group();

  std::string states_string;
  base::FieldTrialList::AllStatesToString(&states_string, false);

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kForceFieldTrials, states_string);
  EXPECT_EQ("*A/G1/B/G2/C/G3/", states_string);

  // Active trial 2 before creating the syncer.
  trial2->group();

  TestFieldTrialObserver observer;
  ChildProcessFieldTrialSyncer syncer(&observer);
  syncer.InitFieldTrialObserving(*base::CommandLine::ForCurrentProcess());

  // The observer should be notified of activated entries that were not activate
  // on the command line. In this case, trial 2. (Trial 1 was already active via
  // command line and so its state shouldn't be notified.)
  ASSERT_EQ(1U, observer.observed_entries_count());
  EXPECT_EQ(MakeStringPair("B", "G2"), observer.get_observed_entry(0));

  // Now, activate trial 3, which should also get reflected.
  trial3->group();
  // Notifications from field trial activation actually happen via posted tasks,
  // so invoke the run loop.
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(2U, observer.observed_entries_count());
  EXPECT_EQ(MakeStringPair("C", "G3"), observer.get_observed_entry(1));
}

}  // namespace variations
