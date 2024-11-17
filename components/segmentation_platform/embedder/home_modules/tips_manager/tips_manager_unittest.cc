// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/tips_manager/tips_manager.h"

#include <optional>

#include "base/json/values_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#include "components/segmentation_platform/embedder/home_modules/tips_manager/signal_constants.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

namespace {

// Tips signal name used to simulate unused signal data in tests.
const std::string kTestSignalName = "test_signal";

// Populates the provided `pref_service` with data representing unused signals.
// This is used to simulate a scenario where the pref service contains
// outdated signal data that needs to be cleaned up.
void PopulatePrefsWithUnusedSignalData(PrefService* pref_service) {
  ScopedDictPrefUpdate update(pref_service, kTipsSignalHistory);

  base::Value::Dict* signal_history = update->EnsureDict(kTestSignalName);

  signal_history->Set(kFirstObservedTime, base::TimeToValue(base::Time::Now()));

  signal_history->Set(kLastObservedTime, base::TimeToValue(base::Time::Now()));

  signal_history->Set(kTotalOccurrences, 1);
}

// Checks if the given signal exists in the pref store and has the specified
// occurrence count.
//
// `signal`: The name of the signal to check.
// `expected_count`: The expected number of occurrences for the signal.
// `pref_service`: The `PrefService` where the signal history is stored.
bool SignalHasExpectedCount(const std::string& signal,
                            int expected_count,
                            PrefService* pref_service) {
  ScopedDictPrefUpdate update(pref_service, kTipsSignalHistory);

  base::Value::Dict* signal_history = update->FindDict(signal);

  if (!signal_history) {
    return false;
  }

  std::optional<int> count = signal_history->FindInt(kTotalOccurrences);

  return count.has_value() && count.value() == expected_count;
}

class TipsManagerStub : public TipsManager {
 public:
  // Constructor.
  explicit TipsManagerStub(PrefService* pref_service,
                           PrefService* local_pref_service)
      : TipsManager(pref_service, local_pref_service) {}

  // `TipsManager` override.
  void HandleInteraction(TipIdentifier tip,
                         TipPresentationContext context) override {}
};

class TipsManagerTest : public testing::Test {
 public:
  void SetUp() override {
    Test::SetUp();

    TipsManager::RegisterProfilePrefs(profile_pref_service_.registry());
    TipsManager::RegisterLocalPrefs(local_pref_service_.registry());
  }

  void TearDown() override { Test::TearDown(); }

  void CreateTipsManager() {
    manager_ = std::make_unique<TipsManagerStub>(&profile_pref_service_,
                                                 &local_pref_service_);
  }

 protected:
  TestingPrefServiceSimple local_pref_service_;
  sync_preferences::TestingPrefServiceSyncable profile_pref_service_;
  std::unique_ptr<TipsManagerStub> manager_;
};

// Verifies that notifying a profile signal for the first time results in a
// signal count of 1 in the profile pref service, and not in the local
// pref service.
TEST_F(TipsManagerTest, NotifyProfileSignalFirstTime) {
  CreateTipsManager();

  EXPECT_TRUE(manager_->NotifySignal(tips_manager::signals::kLensUsed));
  EXPECT_TRUE(SignalHasExpectedCount(tips_manager::signals::kLensUsed, 1,
                                     &profile_pref_service_));
  EXPECT_FALSE(SignalHasExpectedCount(tips_manager::signals::kLensUsed, 1,
                                      &local_pref_service_));
}

// Verifies that notifying a profile signal multiple times increments the signal
// count in the profile pref service accordingly, and not in the local
// pref service.
TEST_F(TipsManagerTest, NotifyProfileSignalMultipleTimes) {
  CreateTipsManager();

  EXPECT_TRUE(manager_->NotifySignal(tips_manager::signals::kLensUsed));
  EXPECT_TRUE(manager_->NotifySignal(tips_manager::signals::kLensUsed));
  EXPECT_TRUE(SignalHasExpectedCount(tips_manager::signals::kLensUsed, 2,
                                     &profile_pref_service_));
  EXPECT_FALSE(SignalHasExpectedCount(tips_manager::signals::kLensUsed, 2,
                                      &local_pref_service_));
}

// Verifies that notifying a local signal for the first time results in a
// signal count of 1 in the local pref service, and not in the profile
// pref service.
TEST_F(TipsManagerTest, NotifyLocalSignalFirstTime) {
  CreateTipsManager();

  EXPECT_TRUE(manager_->NotifySignal(
      tips_manager::signals::kAddressBarPositionChoiceScreenDisplayed));
  EXPECT_TRUE(SignalHasExpectedCount(
      tips_manager::signals::kAddressBarPositionChoiceScreenDisplayed, 1,
      &local_pref_service_));
  EXPECT_FALSE(SignalHasExpectedCount(
      tips_manager::signals::kAddressBarPositionChoiceScreenDisplayed, 1,
      &profile_pref_service_));
}

// Verifies that notifying a local signal multiple times increments the signal
// count in the local pref service accordingly, and not in the profile
// pref service.
TEST_F(TipsManagerTest, NotifyLocalSignalMultipleTimes) {
  CreateTipsManager();

  EXPECT_TRUE(manager_->NotifySignal(
      tips_manager::signals::kAddressBarPositionChoiceScreenDisplayed));
  EXPECT_TRUE(manager_->NotifySignal(
      tips_manager::signals::kAddressBarPositionChoiceScreenDisplayed));
  EXPECT_TRUE(SignalHasExpectedCount(
      tips_manager::signals::kAddressBarPositionChoiceScreenDisplayed, 2,
      &local_pref_service_));
  EXPECT_FALSE(SignalHasExpectedCount(
      tips_manager::signals::kAddressBarPositionChoiceScreenDisplayed, 2,
      &profile_pref_service_));
}

// Verifies that unused signal data is removed from the local pref service.
TEST_F(TipsManagerTest, RemoveUnusedSignalsFromLocalPrefs) {
  PopulatePrefsWithUnusedSignalData(&local_pref_service_);

  ScopedDictPrefUpdate update(&local_pref_service_, kTipsSignalHistory);

  base::Value::Dict* signal_history = update->FindDict(kTestSignalName);

  EXPECT_FALSE(signal_history->empty());

  CreateTipsManager();

  base::Value::Dict* updated_signal_history = update->FindDict(kTestSignalName);

  EXPECT_EQ(updated_signal_history, nullptr);
}

// Verifies that unused signal data is removed from the profile pref service.
TEST_F(TipsManagerTest, RemoveUnusedSignalsFromProfilePrefs) {
  PopulatePrefsWithUnusedSignalData(&profile_pref_service_);

  ScopedDictPrefUpdate update(&profile_pref_service_, kTipsSignalHistory);

  base::Value::Dict* signal_history = update->FindDict(kTestSignalName);

  EXPECT_FALSE(signal_history->empty());

  CreateTipsManager();

  base::Value::Dict* updated_signal_history = update->FindDict(kTestSignalName);

  EXPECT_EQ(updated_signal_history, nullptr);
}

// Verifies that `WasSignalFired()` returns `true` for a signal that has been
// fired.
TEST_F(TipsManagerTest, WasSignalFiredReturnsTrueForFiredSignal) {
  CreateTipsManager();

  EXPECT_TRUE(manager_->NotifySignal(tips_manager::signals::kLensUsed));
  EXPECT_TRUE(manager_->WasSignalFired(tips_manager::signals::kLensUsed));
}

// Verifies that `WasSignalFired()` returns `false` for a signal that has not
// been fired.
TEST_F(TipsManagerTest, WasSignalFiredReturnsFalseForUnfiredSignal) {
  CreateTipsManager();

  EXPECT_FALSE(manager_->WasSignalFired(tips_manager::signals::kLensUsed));
}

// Verifies that `WasSignalFiredWithin()` returns `true` for a signal fired
// within the given window.
TEST_F(TipsManagerTest, WasSignalFiredWithinReturnsTrueForSignalWithinWindow) {
  CreateTipsManager();

  EXPECT_TRUE(manager_->NotifySignal(tips_manager::signals::kLensUsed));
  EXPECT_TRUE(
      manager_->WasSignalFiredWithin(tips_manager::signals::kLensUsed,
                                     base::Minutes(1)));  // Within 1 minute
}

// Verifies that `WasSignalFiredWithin()` returns `false` for a signal fired
// outside the given window.
TEST_F(TipsManagerTest,
       WasSignalFiredWithinReturnsFalseForSignalOutsideWindow) {
  CreateTipsManager();

  EXPECT_TRUE(manager_->NotifySignal(tips_manager::signals::kLensUsed));

  // Simulate the signal being fired a long time ago.
  ScopedDictPrefUpdate update(&profile_pref_service_, kTipsSignalHistory);
  base::Value::Dict* signal_history =
      update->FindDict(tips_manager::signals::kLensUsed);
  ASSERT_TRUE(signal_history);
  signal_history->Set(kLastObservedTime,
                      base::TimeToValue(base::Time::Now() - base::Days(1)));

  EXPECT_FALSE(
      manager_->WasSignalFiredWithin(tips_manager::signals::kLensUsed,
                                     base::Minutes(1)));  // Within 1 minute
}

// Verifies that `WasSignalFiredWithin()` returns false for a signal that has
// not been fired.
TEST_F(TipsManagerTest, WasSignalFiredWithinReturnsFalseForUnfiredSignal) {
  CreateTipsManager();

  EXPECT_FALSE(
      manager_->WasSignalFiredWithin(tips_manager::signals::kLensUsed,
                                     base::Minutes(1)));  // Within 1 minute
}

}  // namespace

}  // namespace segmentation_platform
