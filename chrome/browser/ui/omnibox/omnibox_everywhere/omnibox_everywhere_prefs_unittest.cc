// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"

#include <memory>
#include <utility>

#include "chrome/browser/new_tab_page/prefs/ntp_pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace omnibox_everywhere::prefs {
namespace {

class OmniboxEverywherePrefsTest : public testing::Test {
 public:
  OmniboxEverywherePrefsTest() {
    RegisterLocalStatePrefs(local_state_.registry());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  TestingPrefServiceSimple local_state_;
};

TEST_F(OmniboxEverywherePrefsTest, ShortcutsVisible_FallbackToNtpTrue) {
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, true);
  local_state_.SetInteger(kOmniboxEverywhereShowShortcuts,
                          std::to_underlying(ShowShortcutsPrefValue::kUnset));

  EXPECT_TRUE(IsOmniboxEverywhereShortcutsVisible(&profile_, &local_state_));
}

TEST_F(OmniboxEverywherePrefsTest, ShortcutsVisible_FallbackToNtpFalse) {
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, false);
  local_state_.SetInteger(kOmniboxEverywhereShowShortcuts,
                          std::to_underlying(ShowShortcutsPrefValue::kUnset));

  EXPECT_FALSE(IsOmniboxEverywhereShortcutsVisible(&profile_, &local_state_));
}

TEST_F(OmniboxEverywherePrefsTest, ShortcutsVisible_ExplicitlyEnabled) {
  // Even if NTP shortcuts are disabled, explicit enabled overrides it.
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, false);
  local_state_.SetInteger(kOmniboxEverywhereShowShortcuts,
                          std::to_underlying(ShowShortcutsPrefValue::kEnabled));

  EXPECT_TRUE(IsOmniboxEverywhereShortcutsVisible(&profile_, &local_state_));
}

TEST_F(OmniboxEverywherePrefsTest, ShortcutsVisible_ExplicitlyDisabled) {
  // Even if NTP shortcuts are enabled, explicit disabled overrides it.
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, true);
  local_state_.SetInteger(
      kOmniboxEverywhereShowShortcuts,
      std::to_underlying(ShowShortcutsPrefValue::kDisabled));

  EXPECT_FALSE(IsOmniboxEverywhereShortcutsVisible(&profile_, &local_state_));
}

TEST_F(OmniboxEverywherePrefsTest, ShortcutsVisible_NullLocalStateFallback) {
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, true);
  EXPECT_TRUE(IsOmniboxEverywhereShortcutsVisible(&profile_, nullptr));

  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, false);
  EXPECT_FALSE(IsOmniboxEverywhereShortcutsVisible(&profile_, nullptr));
}

TEST_F(OmniboxEverywherePrefsTest, ShortcutsVisible_NullProfile) {
  local_state_.SetInteger(kOmniboxEverywhereShowShortcuts,
                          std::to_underlying(ShowShortcutsPrefValue::kEnabled));
  EXPECT_TRUE(IsOmniboxEverywhereShortcutsVisible(nullptr, &local_state_));

  local_state_.SetInteger(
      kOmniboxEverywhereShowShortcuts,
      std::to_underlying(ShowShortcutsPrefValue::kDisabled));
  EXPECT_FALSE(IsOmniboxEverywhereShortcutsVisible(nullptr, &local_state_));

  local_state_.SetInteger(kOmniboxEverywhereShowShortcuts,
                          std::to_underlying(ShowShortcutsPrefValue::kUnset));
  EXPECT_FALSE(IsOmniboxEverywhereShortcutsVisible(nullptr, &local_state_));
}

TEST_F(OmniboxEverywherePrefsTest,
       ShortcutsVisible_EnterprisePolicyBothCheckboxesDisabled) {
  // Populate enterprise shortcuts policy list.
  {
    ScopedListPrefUpdate update(
        profile_.GetPrefs(), ntp_tiles::prefs::kEnterpriseShortcutsPolicyList);
    update->Append("https://corp.example.com");
  }

  // Top toggle is ON, but both enterprise and personal shortcuts are OFF.
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, true);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpEnterpriseShortcutsVisible,
                                  false);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpPersonalShortcutsVisible,
                                  false);

  // Even if fallback or explicitly enabled, should return false because no
  // shortcuts exist to show.
  local_state_.SetInteger(kOmniboxEverywhereShowShortcuts,
                          std::to_underlying(ShowShortcutsPrefValue::kUnset));
  EXPECT_FALSE(IsOmniboxEverywhereShortcutsVisible(&profile_, &local_state_));

  local_state_.SetInteger(kOmniboxEverywhereShowShortcuts,
                          std::to_underlying(ShowShortcutsPrefValue::kEnabled));
  EXPECT_FALSE(IsOmniboxEverywhereShortcutsVisible(&profile_, &local_state_));
}

TEST_F(OmniboxEverywherePrefsTest,
       ShortcutsVisible_EnterprisePolicyOnlyPersonalEnabled) {
  {
    ScopedListPrefUpdate update(
        profile_.GetPrefs(), ntp_tiles::prefs::kEnterpriseShortcutsPolicyList);
    update->Append("https://corp.example.com");
  }

  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, true);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpEnterpriseShortcutsVisible,
                                  false);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpPersonalShortcutsVisible,
                                  true);

  local_state_.SetInteger(kOmniboxEverywhereShowShortcuts,
                          std::to_underlying(ShowShortcutsPrefValue::kUnset));
  EXPECT_TRUE(IsOmniboxEverywhereShortcutsVisible(&profile_, &local_state_));
}

TEST_F(OmniboxEverywherePrefsTest,
       ShortcutsVisible_EnterprisePolicyOnlyEnterpriseEnabled) {
  {
    ScopedListPrefUpdate update(
        profile_.GetPrefs(), ntp_tiles::prefs::kEnterpriseShortcutsPolicyList);
    update->Append("https://corp.example.com");
  }

  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, true);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpEnterpriseShortcutsVisible,
                                  true);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpPersonalShortcutsVisible,
                                  false);

  local_state_.SetInteger(kOmniboxEverywhereShowShortcuts,
                          std::to_underlying(ShowShortcutsPrefValue::kUnset));
  EXPECT_TRUE(IsOmniboxEverywhereShortcutsVisible(&profile_, &local_state_));
}

TEST_F(OmniboxEverywherePrefsTest,
       ShortcutsVisible_WithoutEnterprisePolicyShortcutsAlwaysAvailable) {
  // Top toggle is ON, no enterprise policy list.
  // Checkboxes do not appear in Customize Chrome without enterprise policy,
  // so shortcuts remain available even if individual checkbox prefs are false.
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, true);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpPersonalShortcutsVisible,
                                  false);

  local_state_.SetInteger(kOmniboxEverywhereShowShortcuts,
                          std::to_underlying(ShowShortcutsPrefValue::kUnset));
  EXPECT_TRUE(IsOmniboxEverywhereShortcutsVisible(&profile_, &local_state_));

  local_state_.SetInteger(kOmniboxEverywhereShowShortcuts,
                          std::to_underlying(ShowShortcutsPrefValue::kEnabled));
  EXPECT_TRUE(IsOmniboxEverywhereShortcutsVisible(&profile_, &local_state_));
}

TEST_F(OmniboxEverywherePrefsTest, FreStagesProgression_Impressions) {
  // Fresh profile starts at Stage 1 (IntroModal).
  EXPECT_EQ(GetCurrentFreStage(&profile_, &local_state_),
            FreStage::kIntroModal);

  // Increment impression 1
  IncrementFreImpression(&profile_, &local_state_);
  EXPECT_EQ(GetCurrentFreStage(&profile_, &local_state_),
            FreStage::kIntroModal);

  // Increment impression 2 -> Stage 1 reaches max impressions (2).
  // Because default hotkey is enabled and active, Stage 2 (ShortcutSetupChin)
  // is skipped, transitioning directly to Stage 3 (ShortcutReminderChin).
  IncrementFreImpression(&profile_, &local_state_);
  EXPECT_EQ(GetCurrentFreStage(&profile_, &local_state_),
            FreStage::kShortcutReminderChin);

  // Stage 3 impressions (max 3)
  IncrementFreImpression(&profile_, &local_state_);
  EXPECT_EQ(GetCurrentFreStage(&profile_, &local_state_),
            FreStage::kShortcutReminderChin);
  IncrementFreImpression(&profile_, &local_state_);
  EXPECT_EQ(GetCurrentFreStage(&profile_, &local_state_),
            FreStage::kShortcutReminderChin);
  IncrementFreImpression(&profile_, &local_state_);
  // Reached 3 impressions -> FRE complete (kNone) and marked dismissed.
  EXPECT_EQ(GetCurrentFreStage(&profile_, &local_state_), FreStage::kNone);
  EXPECT_TRUE(profile_.GetPrefs()->GetBoolean(kFreDismissed));
}

TEST_F(OmniboxEverywherePrefsTest, FreStagesProgression_ExplicitDismissal) {
  EXPECT_EQ(GetCurrentFreStage(&profile_, &local_state_),
            FreStage::kIntroModal);

  // Dismiss Stage 1 explicitly (e.g. user clicked close 'X').
  // Because default hotkey is enabled and active, Stage 2 is skipped and
  // transitions directly to Stage 3.
  OnFreStageDismissed(&profile_, FreStage::kIntroModal, &local_state_);
  EXPECT_EQ(GetCurrentFreStage(&profile_, &local_state_),
            FreStage::kShortcutReminderChin);

  // Dismiss Stage 3 explicitly
  OnFreStageDismissed(&profile_, FreStage::kShortcutReminderChin,
                      &local_state_);
  EXPECT_EQ(GetCurrentFreStage(&profile_, &local_state_), FreStage::kNone);
  EXPECT_TRUE(profile_.GetPrefs()->GetBoolean(kFreDismissed));
}

TEST_F(OmniboxEverywherePrefsTest,
       FreStagesProgression_NoHotkeyShowsSetupChin) {
  // When hotkey is disabled, HasOmniboxEverywhereHotkey is false.
  local_state_.SetBoolean(kHotkeyEnabled, false);
  EXPECT_FALSE(HasOmniboxEverywhereHotkey(&local_state_));

  // Stage 1 (IntroModal) is shown first.
  EXPECT_EQ(GetCurrentFreStage(&profile_, &local_state_),
            FreStage::kIntroModal);

  // Dismiss Stage 1 explicitly -> transitions to Stage 2 (ShortcutSetupChin).
  OnFreStageDismissed(&profile_, FreStage::kIntroModal, &local_state_);
  EXPECT_EQ(GetCurrentFreStage(&profile_, &local_state_),
            FreStage::kShortcutSetupChin);

  // Dismiss Stage 2 explicitly with no hotkey -> skips Stage 3 and completes
  // FRE.
  OnFreStageDismissed(&profile_, FreStage::kShortcutSetupChin, &local_state_);
  EXPECT_EQ(GetCurrentFreStage(&profile_, &local_state_), FreStage::kNone);
  EXPECT_TRUE(profile_.GetPrefs()->GetBoolean(kFreDismissed));
}

TEST_F(OmniboxEverywherePrefsTest,
       FreStagesProgression_DismissSetupWithHotkeyTransitionsToReminder) {
  // Start with hotkey disabled so that Stage 2 (ShortcutSetupChin) is reached.
  local_state_.SetBoolean(kHotkeyEnabled, false);
  EXPECT_FALSE(HasOmniboxEverywhereHotkey(&local_state_));

  // Dismiss Stage 1 to transition to Stage 2.
  OnFreStageDismissed(&profile_, FreStage::kIntroModal, &local_state_);
  EXPECT_EQ(GetCurrentFreStage(&profile_, &local_state_),
            FreStage::kShortcutSetupChin);

  // Enable a hotkey while in Stage 2.
  local_state_.SetBoolean(kHotkeyEnabled, true);
  EXPECT_TRUE(HasOmniboxEverywhereHotkey(&local_state_));

  // When hotkey is enabled, dismissing Stage 2 transitions to Stage 3.
  OnFreStageDismissed(&profile_, FreStage::kShortcutSetupChin, &local_state_);
  EXPECT_EQ(GetCurrentFreStage(&profile_, &local_state_),
            FreStage::kShortcutReminderChin);
  EXPECT_FALSE(profile_.GetPrefs()->GetBoolean(kFreDismissed));
}

TEST_F(OmniboxEverywherePrefsTest,
       FreStagesProgression_PresetHotkeySkipsSetupChin) {
  EXPECT_TRUE(HasOmniboxEverywhereHotkey(&local_state_));

  // Explicitly configure a hotkey in local state before completing FRE.
  SetOmniboxEverywhereHotkey(&local_state_, "Command+Shift+Space");
  EXPECT_TRUE(HasOmniboxEverywhereHotkey(&local_state_));

  // Stage 1 (IntroModal) is still shown first.
  EXPECT_EQ(GetCurrentFreStage(&profile_, &local_state_),
            FreStage::kIntroModal);

  // Dismiss Stage 1 explicitly -> skips Stage 2 and goes directly to Stage 3.
  OnFreStageDismissed(&profile_, FreStage::kIntroModal, &local_state_);
  EXPECT_EQ(GetCurrentFreStage(&profile_, &local_state_),
            FreStage::kShortcutReminderChin);

  // Dismiss Stage 3 explicitly -> completes FRE.
  OnFreStageDismissed(&profile_, FreStage::kShortcutReminderChin,
                      &local_state_);
  EXPECT_EQ(GetCurrentFreStage(&profile_, &local_state_), FreStage::kNone);
  EXPECT_TRUE(profile_.GetPrefs()->GetBoolean(kFreDismissed));
}

TEST_F(OmniboxEverywherePrefsTest,
       FreStagesProgression_PresetHotkeyImpressionCap) {
  SetOmniboxEverywhereHotkey(&local_state_, "Command+Shift+Space");

  EXPECT_EQ(GetCurrentFreStage(&profile_, &local_state_),
            FreStage::kIntroModal);

  // Exhaust Stage 1 impressions (max 2).
  IncrementFreImpression(&profile_, &local_state_);
  IncrementFreImpression(&profile_, &local_state_);

  // Transitions directly to Stage 3 (ShortcutReminderChin), skipping Stage 2.
  EXPECT_EQ(GetCurrentFreStage(&profile_, &local_state_),
            FreStage::kShortcutReminderChin);
}

TEST_F(OmniboxEverywherePrefsTest, HotkeyPresetsAndTokens) {
  ui::Accelerator default_hotkey(ui::VKEY_SPACE, ui::EF_ALT_DOWN);
  EXPECT_EQ(GetDefaultOmniboxEverywhereHotkey(), default_hotkey);
  EXPECT_EQ(GetOmniboxEverywhereHotkey(&local_state_), default_hotkey);

  auto presets = GetAvailableHotkeyPresets();
  ASSERT_EQ(3u, presets.size());
#if BUILDFLAG(IS_MAC)
  EXPECT_EQ("Alt+Space", presets[0]);
  EXPECT_EQ("Command+Shift+Space", presets[1]);
  EXPECT_EQ("Ctrl+Shift+Space", presets[2]);
#else
  EXPECT_EQ("Alt+Space", presets[0]);
  EXPECT_EQ("Alt+Shift+Space", presets[1]);
  EXPECT_EQ("Alt+O", presets[2]);
#endif

  ui::Accelerator alt_space(ui::VKEY_SPACE, ui::EF_ALT_DOWN);
  auto tokens = GetOmniboxEverywhereHotkeyTokens(alt_space);
  EXPECT_FALSE(tokens.empty());
  EXPECT_EQ(tokens.back(), "Space");
#if BUILDFLAG(IS_MAC)
  EXPECT_EQ(tokens[0], "Option");

  // Verify Apple canonical order [control -> option -> shift -> command].
  ui::Accelerator cmd_shift_space(ui::VKEY_SPACE,
                                  ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN);
  auto cmd_tokens = GetOmniboxEverywhereHotkeyTokens(cmd_shift_space);
  ASSERT_EQ(3u, cmd_tokens.size());
  EXPECT_EQ("Shift", cmd_tokens[0]);
  EXPECT_EQ("Cmd", cmd_tokens[1]);
  EXPECT_EQ("Space", cmd_tokens[2]);

  ui::Accelerator ctrl_shift_space(ui::VKEY_SPACE,
                                   ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  auto ctrl_tokens = GetOmniboxEverywhereHotkeyTokens(ctrl_shift_space);
  ASSERT_EQ(3u, ctrl_tokens.size());
  EXPECT_EQ("Ctrl", ctrl_tokens[0]);
  EXPECT_EQ("Shift", ctrl_tokens[1]);
  EXPECT_EQ("Space", ctrl_tokens[2]);

  // Verify non-letter keys (Return) map to localized text instead of glyphs.
  ui::Accelerator cmd_return(ui::VKEY_RETURN, ui::EF_COMMAND_DOWN);
  auto return_tokens = GetOmniboxEverywhereHotkeyTokens(cmd_return);
  ASSERT_EQ(2u, return_tokens.size());
  EXPECT_EQ("Cmd", return_tokens[0]);
  EXPECT_EQ("Enter", return_tokens[1]);
#else
  EXPECT_EQ(tokens[0], "Alt");
#endif
}

}  // namespace
}  // namespace omnibox_everywhere::prefs
