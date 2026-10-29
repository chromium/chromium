// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_fre_controller.h"

#include <memory>
#include <vector>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace omnibox_everywhere {
namespace {

class OmniboxEverywhereFreControllerTest : public testing::Test {
 protected:
  OmniboxEverywhereFreControllerTest() {
    prefs::RegisterProfilePrefs(profile_prefs_.registry());
    prefs::RegisterLocalStatePrefs(local_state_.registry());
    fre_controller_ = std::make_unique<OmniboxEverywhereFreController>(
        &profile_prefs_, &local_state_);
  }

  base::test::TaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable profile_prefs_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<OmniboxEverywhereFreController> fre_controller_;
};

TEST_F(OmniboxEverywhereFreControllerTest, FreStagesProgression_Impressions) {
  // Fresh profile starts at Stage 1 (IntroModal).
  EXPECT_EQ(fre_controller_->GetCurrentStage(), FreStage::kIntroModal);
  EXPECT_FALSE(fre_controller_->IsFreDismissed());

  // Increment impression 1
  fre_controller_->IncrementImpression();
  EXPECT_EQ(fre_controller_->GetCurrentStage(), FreStage::kIntroModal);

  // Increment impression 2 -> Stage 1 reaches max impressions (2).
  // Because no hotkey is configured by default, advances to Stage 2
  // (ShortcutSetupChin).
  fre_controller_->IncrementImpression();
  EXPECT_EQ(fre_controller_->GetCurrentStage(), FreStage::kShortcutSetupChin);

  // Configure a hotkey while in Stage 2.
  prefs::SetOmniboxEverywhereHotkey(&local_state_, "Ctrl+Shift+Space");
  EXPECT_TRUE(prefs::HasOmniboxEverywhereHotkey(&local_state_));

  // Current open still shows Stage 2.
  EXPECT_EQ(fre_controller_->GetCurrentStage(), FreStage::kShortcutSetupChin);

  // Increment impression 1 (closing the invocation where hotkey was
  // configured). The next invocation immediately shows Stage 3
  // (ShortcutReminderChin).
  fre_controller_->IncrementImpression();
  EXPECT_EQ(fre_controller_->GetCurrentStage(),
            FreStage::kShortcutReminderChin);

  // Stage 3 impressions (max 3)
  fre_controller_->IncrementImpression();
  EXPECT_EQ(fre_controller_->GetCurrentStage(),
            FreStage::kShortcutReminderChin);
  fre_controller_->IncrementImpression();
  EXPECT_EQ(fre_controller_->GetCurrentStage(),
            FreStage::kShortcutReminderChin);
  fre_controller_->IncrementImpression();
  // Reached 3 impressions -> FRE complete (kNone) and marked dismissed.
  EXPECT_EQ(fre_controller_->GetCurrentStage(), FreStage::kNone);
  EXPECT_TRUE(fre_controller_->IsFreDismissed());
  EXPECT_TRUE(profile_prefs_.GetBoolean(prefs::kFreDismissed));
}

TEST_F(OmniboxEverywhereFreControllerTest,
       FreStagesProgression_ExplicitDismissal) {
  EXPECT_EQ(fre_controller_->GetCurrentStage(), FreStage::kIntroModal);

  // Dismiss Stage 1 explicitly (e.g. user clicked close 'X').
  // Because no hotkey is configured by default, advances to Stage 2
  // (ShortcutSetupChin).
  fre_controller_->OnStageDismissed(FreStage::kIntroModal);
  EXPECT_EQ(fre_controller_->GetCurrentStage(), FreStage::kShortcutSetupChin);

  // Dismiss Stage 2 explicitly with no hotkey -> completes FRE.
  fre_controller_->OnStageDismissed(FreStage::kShortcutSetupChin);
  EXPECT_EQ(fre_controller_->GetCurrentStage(), FreStage::kNone);
  EXPECT_TRUE(fre_controller_->IsFreDismissed());
  EXPECT_TRUE(profile_prefs_.GetBoolean(prefs::kFreDismissed));
}

TEST_F(OmniboxEverywhereFreControllerTest,
       FreStagesProgression_NoHotkeyShowsSetupChin) {
  // When hotkey is disabled, HasOmniboxEverywhereHotkey is false.
  local_state_.SetBoolean(prefs::kHotkeyEnabled, false);
  EXPECT_FALSE(prefs::HasOmniboxEverywhereHotkey(&local_state_));

  // Stage 1 (IntroModal) is shown first.
  EXPECT_EQ(fre_controller_->GetCurrentStage(), FreStage::kIntroModal);

  // Dismiss Stage 1 explicitly -> transitions to Stage 2 (ShortcutSetupChin).
  fre_controller_->OnStageDismissed(FreStage::kIntroModal);
  EXPECT_EQ(fre_controller_->GetCurrentStage(), FreStage::kShortcutSetupChin);

  // Dismiss Stage 2 explicitly with no hotkey -> skips Stage 3 and completes
  // FRE.
  fre_controller_->OnStageDismissed(FreStage::kShortcutSetupChin);
  EXPECT_EQ(fre_controller_->GetCurrentStage(), FreStage::kNone);
  EXPECT_TRUE(fre_controller_->IsFreDismissed());
  EXPECT_TRUE(profile_prefs_.GetBoolean(prefs::kFreDismissed));
}

TEST_F(OmniboxEverywhereFreControllerTest,
       FreStagesProgression_DismissSetupWithHotkeySkipsReminder) {
  EXPECT_FALSE(prefs::HasOmniboxEverywhereHotkey(&local_state_));

  // Dismiss Stage 1 to transition to Stage 2.
  fre_controller_->OnStageDismissed(FreStage::kIntroModal);
  EXPECT_EQ(fre_controller_->GetCurrentStage(), FreStage::kShortcutSetupChin);

  // Configure a hotkey while in Stage 2.
  prefs::SetOmniboxEverywhereHotkey(&local_state_, "Ctrl+Shift+Space");
  EXPECT_TRUE(prefs::HasOmniboxEverywhereHotkey(&local_state_));
  // Does not immediately switch to Stage 3 before dismissal or impression cap.
  EXPECT_EQ(fre_controller_->GetCurrentStage(), FreStage::kShortcutSetupChin);

  // When user dismisses Stage 2 after selecting a hotkey, Stage 3 is skipped
  // and FRE is marked dismissed.
  fre_controller_->OnStageDismissed(FreStage::kShortcutSetupChin);
  EXPECT_EQ(fre_controller_->GetCurrentStage(), FreStage::kNone);
  EXPECT_TRUE(fre_controller_->IsFreDismissed());
  EXPECT_TRUE(profile_prefs_.GetBoolean(prefs::kFreDismissed));
  EXPECT_TRUE(profile_prefs_.GetBoolean(prefs::kFreShortcutReminderDismissed));
}

TEST_F(OmniboxEverywhereFreControllerTest, NullProfilePrefsSafety) {
  OmniboxEverywhereFreController null_controller(nullptr, &local_state_);
  EXPECT_EQ(null_controller.GetCurrentStage(), FreStage::kNone);
  EXPECT_TRUE(null_controller.IsFreDismissed());
  EXPECT_NO_FATAL_FAILURE(null_controller.IncrementImpression());
  EXPECT_NO_FATAL_FAILURE(
      null_controller.OnStageDismissed(FreStage::kIntroModal));
  EXPECT_NO_FATAL_FAILURE(
      null_controller.MarkStageCompleted(FreStage::kShortcutSetupChin));
}

TEST_F(OmniboxEverywhereFreControllerTest,
       MarkStageCompleted_SetupChinPreservesReminder) {
  // Dismiss Stage 1 to transition to Stage 2.
  fre_controller_->OnStageDismissed(FreStage::kIntroModal);
  EXPECT_EQ(fre_controller_->GetCurrentStage(), FreStage::kShortcutSetupChin);

  // User sets hotkey (e.g. in Settings) and marks Stage 2 completed.
  prefs::SetOmniboxEverywhereHotkey(&local_state_, "Ctrl+Shift+Space");
  fre_controller_->MarkStageCompleted(FreStage::kShortcutSetupChin);

  // Stage 3 (reminder chin) is shown, not skipped.
  EXPECT_EQ(fre_controller_->GetCurrentStage(),
            FreStage::kShortcutReminderChin);
  EXPECT_FALSE(fre_controller_->IsFreDismissed());
  EXPECT_FALSE(profile_prefs_.GetBoolean(prefs::kFreDismissed));
  EXPECT_FALSE(profile_prefs_.GetBoolean(prefs::kFreShortcutReminderDismissed));
}

TEST_F(OmniboxEverywhereFreControllerTest,
       FreStagesProgression_SetupHotkeyImpressionTransitionsToReminder) {
  // Start with hotkey disabled so that Stage 2 (ShortcutSetupChin) is reached.
  local_state_.SetBoolean(prefs::kHotkeyEnabled, false);
  EXPECT_FALSE(prefs::HasOmniboxEverywhereHotkey(&local_state_));

  // Dismiss Stage 1 to transition to Stage 2.
  fre_controller_->OnStageDismissed(FreStage::kIntroModal);
  EXPECT_EQ(fre_controller_->GetCurrentStage(), FreStage::kShortcutSetupChin);

  // While no hotkey is selected, impressions are not capped.
  for (int i = 0; i < 5; ++i) {
    fre_controller_->IncrementImpression();
    EXPECT_EQ(fre_controller_->GetCurrentStage(), FreStage::kShortcutSetupChin);
  }
  EXPECT_EQ(profile_prefs_.GetInteger(prefs::kFreShortcutSetupImpressionCount),
            0);

  // Set a hotkey while in Stage 2.
  prefs::SetOmniboxEverywhereHotkey(&local_state_, "Ctrl+Shift+Space");
  EXPECT_TRUE(prefs::HasOmniboxEverywhereHotkey(&local_state_));

  // Does not immediately switch to Stage 3 during current open.
  EXPECT_EQ(fre_controller_->GetCurrentStage(), FreStage::kShortcutSetupChin);

  // Increment impression 1 -> reaches impression cap (1), advances to
  // Stage 3 on the next invocation.
  fre_controller_->IncrementImpression();
  EXPECT_EQ(fre_controller_->GetCurrentStage(),
            FreStage::kShortcutReminderChin);
  EXPECT_FALSE(fre_controller_->IsFreDismissed());
}

TEST_F(OmniboxEverywhereFreControllerTest,
       FreStagesProgression_PresetHotkeySkipsSetupChin) {
  EXPECT_FALSE(prefs::HasOmniboxEverywhereHotkey(&local_state_));

  // Explicitly configure a hotkey in local state before completing FRE.
  prefs::SetOmniboxEverywhereHotkey(&local_state_, "Ctrl+Shift+Space");
  EXPECT_TRUE(prefs::HasOmniboxEverywhereHotkey(&local_state_));

  // Stage 1 (IntroModal) is still shown first.
  EXPECT_EQ(fre_controller_->GetCurrentStage(), FreStage::kIntroModal);

  // Dismiss Stage 1 explicitly -> skips Stage 2 and goes directly to Stage 3.
  fre_controller_->OnStageDismissed(FreStage::kIntroModal);
  EXPECT_EQ(fre_controller_->GetCurrentStage(),
            FreStage::kShortcutReminderChin);

  // Dismiss Stage 3 explicitly -> completes FRE.
  fre_controller_->OnStageDismissed(FreStage::kShortcutReminderChin);
  EXPECT_EQ(fre_controller_->GetCurrentStage(), FreStage::kNone);
  EXPECT_TRUE(fre_controller_->IsFreDismissed());
}

TEST_F(OmniboxEverywhereFreControllerTest,
       FreStagesProgression_PresetHotkeyImpressionCap) {
  prefs::SetOmniboxEverywhereHotkey(&local_state_, "Ctrl+Shift+Space");

  EXPECT_EQ(fre_controller_->GetCurrentStage(), FreStage::kIntroModal);

  // Exhaust Stage 1 impressions (max 2).
  fre_controller_->IncrementImpression();
  fre_controller_->IncrementImpression();

  // Transitions directly to Stage 3 (ShortcutReminderChin), skipping Stage 2.
  EXPECT_EQ(fre_controller_->GetCurrentStage(),
            FreStage::kShortcutReminderChin);
}

TEST_F(OmniboxEverywhereFreControllerTest,
       FreStagesProgression_CustomFinchImpressionCaps) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kOmniboxEverywhereFre,
      {{omnibox::kOmniboxEverywhereFreMaxIntroImpressionsParam.name, "1"},
       {omnibox::kOmniboxEverywhereFreMaxShortcutSetupImpressionsParam.name,
        "2"},
       {omnibox::kOmniboxEverywhereFreMaxShortcutReminderImpressionsParam.name,
        "1"}});

  // Stage 1: Custom cap of 1 impression.
  EXPECT_EQ(fre_controller_->GetCurrentStage(), FreStage::kIntroModal);
  fre_controller_->IncrementImpression();
  EXPECT_EQ(fre_controller_->GetCurrentStage(), FreStage::kShortcutSetupChin);

  // Stage 2: Custom cap of 2 impressions after hotkey is set.
  prefs::SetOmniboxEverywhereHotkey(&local_state_, "Ctrl+Shift+Space");
  fre_controller_->IncrementImpression();
  EXPECT_EQ(fre_controller_->GetCurrentStage(), FreStage::kShortcutSetupChin);
  fre_controller_->IncrementImpression();
  EXPECT_EQ(fre_controller_->GetCurrentStage(),
            FreStage::kShortcutReminderChin);

  // Stage 3: Custom cap of 1 impression.
  fre_controller_->IncrementImpression();
  EXPECT_EQ(fre_controller_->GetCurrentStage(), FreStage::kNone);
  EXPECT_TRUE(fre_controller_->IsFreDismissed());
  EXPECT_TRUE(profile_prefs_.GetBoolean(prefs::kFreDismissed));
}

TEST_F(OmniboxEverywhereFreControllerTest, FeatureFlagDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(omnibox::kOmniboxEverywhereFre);

  EXPECT_EQ(fre_controller_->GetCurrentStage(), FreStage::kNone);
  EXPECT_TRUE(fre_controller_->IsFreDismissed());

  fre_controller_->IncrementImpression();
  EXPECT_EQ(profile_prefs_.GetInteger(prefs::kFreIntroImpressionCount), 0);

  fre_controller_->OnStageDismissed(FreStage::kIntroModal);
  EXPECT_FALSE(profile_prefs_.GetBoolean(prefs::kFreIntroDismissed));

  fre_controller_->MarkStageCompleted(FreStage::kShortcutSetupChin);
  EXPECT_FALSE(profile_prefs_.GetBoolean(prefs::kFreShortcutSetupDismissed));
}

TEST_F(OmniboxEverywhereFreControllerTest,
       StateChangedCallbackFiresOnProfileAndLocalStatePrefChanges) {
  int notification_count = 0;
  base::CallbackListSubscription subscription =
      fre_controller_->RegisterStateChangedCallback(
          base::BindLambdaForTesting([&]() { ++notification_count; }));

  // Profile pref changes notify observers.
  profile_prefs_.SetInteger(prefs::kFreIntroImpressionCount, 1);
  EXPECT_EQ(notification_count, 1);

  profile_prefs_.SetBoolean(prefs::kFreIntroDismissed, true);
  EXPECT_EQ(notification_count, 2);

  // Local state hotkey pref changes notify observers.
  local_state_.SetString(prefs::kOmniboxEverywhereHotkey, "Alt+Space");
  EXPECT_EQ(notification_count, 3);

  local_state_.SetBoolean(prefs::kHotkeyEnabled, false);
  EXPECT_EQ(notification_count, 4);

  // Resetting subscription stops notifications.
  subscription = {};
  profile_prefs_.SetBoolean(prefs::kFreDismissed, true);
  EXPECT_EQ(notification_count, 4);
}

TEST_F(OmniboxEverywhereFreControllerTest,
       MarkStageCompleted_IntroAndReminderAndNoneStages) {
  EXPECT_EQ(fre_controller_->GetCurrentStage(), FreStage::kIntroModal);

  // Completing Stage 1 advances to Stage 2 when no hotkey is configured.
  fre_controller_->MarkStageCompleted(FreStage::kIntroModal);
  EXPECT_TRUE(profile_prefs_.GetBoolean(prefs::kFreIntroDismissed));
  EXPECT_EQ(fre_controller_->GetCurrentStage(), FreStage::kShortcutSetupChin);

  // Configure a hotkey and complete Stage 2 -> advances to Stage 3.
  prefs::SetOmniboxEverywhereHotkey(&local_state_, "Ctrl+Shift+Space");
  fre_controller_->MarkStageCompleted(FreStage::kShortcutSetupChin);
  EXPECT_EQ(fre_controller_->GetCurrentStage(),
            FreStage::kShortcutReminderChin);

  // Completing kNone is a no-op.
  fre_controller_->MarkStageCompleted(FreStage::kNone);
  EXPECT_EQ(fre_controller_->GetCurrentStage(),
            FreStage::kShortcutReminderChin);
  EXPECT_FALSE(fre_controller_->IsFreDismissed());

  // Completing Stage 3 marks both reminder and overall FRE as dismissed.
  fre_controller_->MarkStageCompleted(FreStage::kShortcutReminderChin);
  EXPECT_EQ(fre_controller_->GetCurrentStage(), FreStage::kNone);
  EXPECT_TRUE(fre_controller_->IsFreDismissed());
  EXPECT_TRUE(profile_prefs_.GetBoolean(prefs::kFreShortcutReminderDismissed));
  EXPECT_TRUE(profile_prefs_.GetBoolean(prefs::kFreDismissed));
}

TEST_F(OmniboxEverywhereFreControllerTest, OnStageDismissed_NoneStage) {
  EXPECT_FALSE(fre_controller_->IsFreDismissed());
  fre_controller_->OnStageDismissed(FreStage::kNone);
  EXPECT_EQ(fre_controller_->GetCurrentStage(), FreStage::kNone);
  EXPECT_TRUE(fre_controller_->IsFreDismissed());
  EXPECT_TRUE(profile_prefs_.GetBoolean(prefs::kFreDismissed));
}

TEST_F(OmniboxEverywhereFreControllerTest,
       DisablingHotkeyDuringReminderStageUpdatesCurrentStage) {
  prefs::SetOmniboxEverywhereHotkey(&local_state_, "Ctrl+Shift+Space");
  fre_controller_->OnStageDismissed(FreStage::kIntroModal);
  // Stage 2 was skipped because a hotkey was preset; starts at Stage 3.
  EXPECT_EQ(fre_controller_->GetCurrentStage(),
            FreStage::kShortcutReminderChin);

  // Disabling the hotkey while in Stage 3 (when Stage 2 was already dismissed)
  // causes Stage 3 to become inactive (kNone) until re-enabled.
  local_state_.SetBoolean(prefs::kHotkeyEnabled, false);
  EXPECT_EQ(fre_controller_->GetCurrentStage(), FreStage::kNone);

  // Re-enabling the hotkey restores Stage 3 since reminder impressions remain.
  local_state_.SetBoolean(prefs::kHotkeyEnabled, true);
  EXPECT_EQ(fre_controller_->GetCurrentStage(),
            FreStage::kShortcutReminderChin);
}

TEST_F(OmniboxEverywhereFreControllerTest,
       MultiPrefTransitionsNeverEmitTransientStage) {
  prefs::SetOmniboxEverywhereHotkey(&local_state_, "Ctrl+Shift+Space");

  std::vector<FreStage> observed_stages;
  base::CallbackListSubscription subscription =
      fre_controller_->RegisterStateChangedCallback(
          base::BindLambdaForTesting([&]() {
            observed_stages.push_back(fre_controller_->GetCurrentStage());
          }));

  // 1. Dismissing Stage 1 with a preset hotkey skips Stage 2 and must never
  // transiently report FreStage::kShortcutSetupChin.
  fre_controller_->OnStageDismissed(FreStage::kIntroModal);
  for (FreStage stage : observed_stages) {
    EXPECT_NE(stage, FreStage::kShortcutSetupChin);
  }
  EXPECT_EQ(fre_controller_->GetCurrentStage(),
            FreStage::kShortcutReminderChin);

  // 2. Reset Stage 2 to active with a hotkey set, then dismiss Stage 2. It
  // skips Stage 3 and must never transiently report
  // FreStage::kShortcutReminderChin.
  profile_prefs_.SetBoolean(prefs::kFreShortcutSetupDismissed, false);
  observed_stages.clear();
  fre_controller_->OnStageDismissed(FreStage::kShortcutSetupChin);
  for (FreStage stage : observed_stages) {
    EXPECT_NE(stage, FreStage::kShortcutReminderChin);
  }
  EXPECT_EQ(fre_controller_->GetCurrentStage(), FreStage::kNone);
}

}  // namespace
}  // namespace omnibox_everywhere
