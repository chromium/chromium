// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/sticky_activation_manager.h"

#include "base/feature_list.h"
#include "base/metrics/metrics_hashes.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {
namespace {

class StickyActivationManagerTest : public testing::Test {
 protected:
  StickyActivationManagerTest() {
    StickyActivationManager::RegisterPrefs(*local_state_.registry());
  }

  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple local_state_;
};

TEST_F(StickyActivationManagerTest, ShouldActivate) {
  local_state_.SetString(prefs::kVariationsStickyStudies, "Foo/Bar/Foo2/Bar2");
  StickyActivationManager manager(&local_state_);

  EXPECT_TRUE(manager.ShouldActivate("Foo", "Bar"));
  EXPECT_TRUE(manager.ShouldActivate("Foo2", "Bar2"));
  EXPECT_FALSE(manager.ShouldActivate("Foo3", "Bar3"));
}

TEST_F(StickyActivationManagerTest, SavePrefs) {
  local_state_.SetString(prefs::kVariationsStickyStudies, "Foo/Bar/Foo2/Bar2");
  StickyActivationManager manager(&local_state_);

  auto* foo_far_trial = base::FieldTrialList::CreateFieldTrial("Foo", "Far");
  auto* bla_bla_trial = base::FieldTrialList::CreateFieldTrial("Bla", "Bla");

  // Note: The ShouldActivate() calls mark the trials for monitoring.
  EXPECT_FALSE(manager.ShouldActivate("Foo", "Far"));
  EXPECT_TRUE(manager.ShouldActivate("Foo2", "Bar2"));
  EXPECT_FALSE(manager.ShouldActivate("Foo3", "Bar3"));
  manager.StartMonitoring();
  // Only Foo2/Bar2 should be written, as the other ones did not match.
  EXPECT_EQ("Foo2/Bar2",
            local_state_.GetString(prefs::kVariationsStickyStudies));

  // Activate both "Foo" and "Bla" trials. Only "Foo" should be marked since
  // "Bla" hasn't been queried using ShouldActivate() and thus isn't sticky.
  foo_far_trial->group_name();
  bla_bla_trial->group_name();
  // The pref should have been updated with Foo added and Foo2 still there.
  EXPECT_EQ("Foo/Far/Foo2/Bar2",
            local_state_.GetString(prefs::kVariationsStickyStudies));
}

TEST_F(StickyActivationManagerTest, NoCrashAfterManagerDestroyed) {
  auto* foo_trial = base::FieldTrialList::CreateFieldTrial("Foo", "Bar");
  auto* foo2_trial = base::FieldTrialList::CreateFieldTrial("Foo2", "Bar2");

  {
    StickyActivationManager manager(&local_state_);
    // Note: The ShouldActivate() calls mark the trials for monitoring.
    EXPECT_FALSE(manager.ShouldActivate("Foo", "Far"));
    EXPECT_FALSE(manager.ShouldActivate("Foo2", "Bar2"));
    manager.StartMonitoring();

    // Activate the first trial.
    foo_trial->group_name();
    // It should be saved to prefs.
    EXPECT_EQ("Foo/Bar",
              local_state_.GetString(prefs::kVariationsStickyStudies));
  }

  // Activate the second trial after the manager is destroyed. This shouldn't
  // crash and can happen in practice during shutdown, as different objects get
  // destroyed at different times, and their destruction may do feature checks.
  foo2_trial->group_name();

  // The pref should not have changed, as the manager should have stopped
  // observing.
  EXPECT_EQ("Foo/Bar",
            local_state_.GetString(prefs::kVariationsStickyStudies));
}

TEST_F(StickyActivationManagerTest, ParseInvalidPref) {
  local_state_.SetString(prefs::kVariationsStickyStudies, "Foo/Bar/Baz");
  StickyActivationManager manager(&local_state_);

  EXPECT_FALSE(manager.ShouldActivate("Foo", "Bar"));
  EXPECT_FALSE(manager.ShouldActivate("Baz", ""));
}

TEST_F(StickyActivationManagerTest, GroupChanged) {
  local_state_.SetString(prefs::kVariationsStickyStudies, "Foo/Bar");
  StickyActivationManager manager(&local_state_);

  auto* foo_baz_trial = base::FieldTrialList::CreateFieldTrial("Foo", "Baz");

  // "Foo" is sticky, but for group "Bar". "Baz" should not be sticky.
  EXPECT_FALSE(manager.ShouldActivate("Foo", "Baz"));
  manager.StartMonitoring();

  // The pref should be empty now, as nothing was activated.
  EXPECT_EQ("", local_state_.GetString(prefs::kVariationsStickyStudies));

  // Finalize the "Foo" trial with the "Baz" group.
  foo_baz_trial->group_name();

  // The pref should now contain the new sticky group.
  EXPECT_EQ("Foo/Baz",
            local_state_.GetString(prefs::kVariationsStickyStudies));
}

TEST_F(StickyActivationManagerTest, NullLocalState) {
  // Verifies that the manager works fine with a null local_state.
  StickyActivationManager manager(nullptr);
  EXPECT_FALSE(manager.ShouldActivate("Foo", "Bar"));
  manager.StartMonitoring();
  // Should not crash.

  auto* foo_bar_trial = base::FieldTrialList::CreateFieldTrial("Foo", "Bar");
  foo_bar_trial->group_name();
  // Should not crash.
}

TEST_F(StickyActivationManagerTest, RecordActivation) {
  base::HistogramTester histogram_tester;
  StickyActivationManager manager(&local_state_);

  auto* trial = base::FieldTrialList::CreateFieldTrial("Foo", "Bar");

  // Mark "Foo" as a sticky trial.
  EXPECT_FALSE(manager.ShouldActivate("Foo", "Bar"));
  manager.StartMonitoring();

  // Activate the trial.
  trial->group_name();

  histogram_tester.ExpectUniqueSample("Variations.StickyAfterQuery.Activation",
                                      base::HashFieldTrialName("Foo"), 1);
}

TEST_F(StickyActivationManagerTest, NoActivationRecordingBeforeMonitoring) {
  base::HistogramTester histogram_tester;

  // Set "Foo" as a sticky trial with group "Bar".
  local_state_.SetString(prefs::kVariationsStickyStudies, "Foo/Bar");
  StickyActivationManager manager(&local_state_);

  auto* trial = base::FieldTrialList::CreateFieldTrial("Foo", "Bar");

  // ShouldActivate() should return true.
  EXPECT_TRUE(manager.ShouldActivate("Foo", "Bar"));

  // Activate the trial before monitoring starts.
  trial->group_name();

  manager.StartMonitoring();

  // No activation should be recorded because the finalization happened before
  // the manager started observing.
  histogram_tester.ExpectTotalCount("Variations.StickyAfterQuery.Activation",
                                    0);
}

// During startup, the feature list is not yet initialized. This verifies that
// the expected use flow for the sticky activation manager doesn't crash due
// to that.
TEST_F(StickyActivationManagerTest, HandlesNullFeatureList) {
  std::unique_ptr<base::FeatureList> original_feature_list =
      base::FeatureList::ClearInstanceForTesting();
  EXPECT_EQ(nullptr, base::FeatureList::GetInstance());
  // Note: This matches what the production Chrome startup path does.
  base::FeatureList::FailOnFeatureAccessWithoutFeatureList();

  // Set "Trial1/Foo" and "Trial2/Bar" as sticky trials.
  local_state_.SetString(prefs::kVariationsStickyStudies,
                         "Trial1/Foo/Trial2/Bar");
  StickyActivationManager manager(&local_state_);

  // Matches pref, so should activate.
  auto* trial1 = base::FieldTrialList::CreateFieldTrial("Trial1", "Foo");
  EXPECT_TRUE(manager.ShouldActivate("Trial1", "Foo"));
  trial1->Activate();

  // Doesn't match pref, so should not activate.
  auto* trial2 = base::FieldTrialList::CreateFieldTrial("Trial2", "Foo");
  EXPECT_FALSE(manager.ShouldActivate("Trial2", "Foo"));

  // Not in pref, so should not activate.
  auto* trial3 = base::FieldTrialList::CreateFieldTrial("Trial3", "Baz");
  EXPECT_FALSE(manager.ShouldActivate("Trial3", "Baz"));

  // Starting monitoring and activation of trials before feature list init
  // shouldn't crash. Note: Typically, trials should be activated after the
  // feature list is initialized, but the below can still happen as we still
  // run some internal variations code before the feature list is initialized.
  manager.StartMonitoring();
  trial2->group_name();
  EXPECT_EQ("Trial1/Foo/Trial2/Foo",
            local_state_.GetString(prefs::kVariationsStickyStudies));
  trial3->group_name();
  EXPECT_EQ("Trial1/Foo/Trial2/Foo/Trial3/Baz",
            local_state_.GetString(prefs::kVariationsStickyStudies));

  // Call ClearInstanceForTesting() to reset the EarlyFeatureAccessTracker to
  // undo FailOnFeatureAccessWithoutFeatureList() above.
  base::FeatureList::ClearInstanceForTesting();
  base::FeatureList::RestoreInstanceForTesting(
      std::move(original_feature_list));
}

TEST_F(StickyActivationManagerTest, TrialActivationOnDifferentThread) {
  StickyActivationManager manager(&local_state_);

  auto* trial = base::FieldTrialList::CreateFieldTrial("Foo", "Bar");

  // Mark "Foo" as a sticky trial.
  EXPECT_FALSE(manager.ShouldActivate("Foo", "Bar"));
  manager.StartMonitoring();

  // Activate the trial on another thread. This simulates how this might run in
  // production, as the observer can be called from any thread.
  base::RunLoop run_loop;
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](base::FieldTrial* trial, base::OnceClosure quit_closure) {
            trial->group_name();
            std::move(quit_closure).Run();
          },
          base::Unretained(trial), run_loop.QuitClosure()));
  run_loop.Run();

  // The pref should have been updated with Foo added.
  task_environment_.RunUntilIdle();
  EXPECT_EQ("Foo/Bar", local_state_.GetString(prefs::kVariationsStickyStudies));
}

}  // namespace
}  // namespace variations
