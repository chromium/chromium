// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/new_tab_page/prefs/ntp_pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/ntp_tiles/enterprise/enterprise_shortcuts_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/command.h"
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
  profile_.GetPrefs()->SetInteger(
      kOmniboxEverywhereShowShortcuts,
      std::to_underlying(ShowShortcutsPrefValue::kUnset));

  EXPECT_TRUE(IsOmniboxEverywhereShortcutsVisible(&profile_));
}

TEST_F(OmniboxEverywherePrefsTest, ShortcutsVisible_FallbackToNtpFalse) {
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, false);
  profile_.GetPrefs()->SetInteger(
      kOmniboxEverywhereShowShortcuts,
      std::to_underlying(ShowShortcutsPrefValue::kUnset));

  EXPECT_FALSE(IsOmniboxEverywhereShortcutsVisible(&profile_));
}

TEST_F(OmniboxEverywherePrefsTest, ShortcutsVisible_ExplicitlyEnabled) {
  // Even if NTP shortcuts are disabled, explicit enabled overrides it.
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, false);
  profile_.GetPrefs()->SetInteger(
      kOmniboxEverywhereShowShortcuts,
      std::to_underlying(ShowShortcutsPrefValue::kEnabled));

  EXPECT_TRUE(IsOmniboxEverywhereShortcutsVisible(&profile_));
}

TEST_F(OmniboxEverywherePrefsTest, ShortcutsVisible_ExplicitlyDisabled) {
  // Even if NTP shortcuts are enabled, explicit disabled overrides it.
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, true);
  profile_.GetPrefs()->SetInteger(
      kOmniboxEverywhereShowShortcuts,
      std::to_underlying(ShowShortcutsPrefValue::kDisabled));

  EXPECT_FALSE(IsOmniboxEverywhereShortcutsVisible(&profile_));
}

TEST_F(OmniboxEverywherePrefsTest, ShortcutsVisible_NullProfile) {
  EXPECT_FALSE(IsOmniboxEverywhereShortcutsVisible(nullptr));
}

TEST_F(OmniboxEverywherePrefsTest, ShortcutsVisible_ProfileScopedIsolation) {
  TestingProfile profile2;
  profile_.GetPrefs()->SetInteger(
      kOmniboxEverywhereShowShortcuts,
      std::to_underlying(ShowShortcutsPrefValue::kDisabled));
  profile2.GetPrefs()->SetInteger(
      kOmniboxEverywhereShowShortcuts,
      std::to_underlying(ShowShortcutsPrefValue::kEnabled));

  EXPECT_FALSE(IsOmniboxEverywhereShortcutsVisible(&profile_));
  EXPECT_TRUE(IsOmniboxEverywhereShortcutsVisible(&profile2));
}

TEST_F(OmniboxEverywherePrefsTest,
       ShortcutsVisible_EnterprisePolicyBothCheckboxesDisabled) {
  // Populate enterprise shortcuts policy list.
  {
    ScopedListPrefUpdate update(
        profile_.GetPrefs(), ntp_tiles::prefs::kEnterpriseShortcutsPolicyList);
    update->Append(base::DictValue()
                       .Set("url", "https://corp.example.com")
                       .Set("title", "Corp"));
  }

  // Top toggle is ON, but both enterprise and personal shortcuts are OFF.
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, true);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpEnterpriseShortcutsVisible,
                                  false);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpPersonalShortcutsVisible,
                                  false);

  // Even if fallback or explicitly enabled, should return false because no
  // shortcuts exist to show.
  profile_.GetPrefs()->SetInteger(
      kOmniboxEverywhereShowShortcuts,
      std::to_underlying(ShowShortcutsPrefValue::kUnset));
  EXPECT_FALSE(IsOmniboxEverywhereShortcutsVisible(&profile_));

  profile_.GetPrefs()->SetInteger(
      kOmniboxEverywhereShowShortcuts,
      std::to_underlying(ShowShortcutsPrefValue::kEnabled));
  EXPECT_FALSE(IsOmniboxEverywhereShortcutsVisible(&profile_));
}

TEST_F(OmniboxEverywherePrefsTest,
       ShortcutsVisible_EnterprisePolicyOnlyPersonalEnabled) {
  {
    ScopedListPrefUpdate update(
        profile_.GetPrefs(), ntp_tiles::prefs::kEnterpriseShortcutsPolicyList);
    update->Append(base::DictValue()
                       .Set("url", "https://corp.example.com")
                       .Set("title", "Corp"));
  }

  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, true);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpEnterpriseShortcutsVisible,
                                  false);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpPersonalShortcutsVisible,
                                  true);

  profile_.GetPrefs()->SetInteger(
      kOmniboxEverywhereShowShortcuts,
      std::to_underlying(ShowShortcutsPrefValue::kUnset));
  EXPECT_TRUE(IsOmniboxEverywhereShortcutsVisible(&profile_));
}

TEST_F(OmniboxEverywherePrefsTest,
       ShortcutsVisible_EnterprisePolicyOnlyEnterpriseEnabled) {
  {
    ScopedListPrefUpdate update(
        profile_.GetPrefs(), ntp_tiles::prefs::kEnterpriseShortcutsPolicyList);
    update->Append(base::DictValue()
                       .Set("url", "https://corp.example.com")
                       .Set("title", "Corp"));
  }

  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, true);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpEnterpriseShortcutsVisible,
                                  true);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpPersonalShortcutsVisible,
                                  false);

  profile_.GetPrefs()->SetInteger(
      kOmniboxEverywhereShowShortcuts,
      std::to_underlying(ShowShortcutsPrefValue::kUnset));
  EXPECT_TRUE(IsOmniboxEverywhereShortcutsVisible(&profile_));
}

TEST_F(OmniboxEverywherePrefsTest,
       ShortcutsVisible_WithoutEnterprisePolicyShortcutsAlwaysAvailable) {
  // Top toggle is ON, no enterprise policy list.
  // Checkboxes do not appear in Customize Chrome without enterprise policy,
  // so shortcuts remain available even if individual checkbox prefs are false.
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, true);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpPersonalShortcutsVisible,
                                  false);

  profile_.GetPrefs()->SetInteger(
      kOmniboxEverywhereShowShortcuts,
      std::to_underlying(ShowShortcutsPrefValue::kUnset));
  EXPECT_TRUE(IsOmniboxEverywhereShortcutsVisible(&profile_));

  profile_.GetPrefs()->SetInteger(
      kOmniboxEverywhereShowShortcuts,
      std::to_underlying(ShowShortcutsPrefValue::kEnabled));
  EXPECT_TRUE(IsOmniboxEverywhereShortcutsVisible(&profile_));
}

TEST_F(OmniboxEverywherePrefsTest,
       ShortcutsVisible_MalformedEnterprisePolicyTreatedAsNoPolicy) {
  // A policy entry without a URL is not a valid enterprise shortcut, so it
  // must not gate shortcut availability on the enterprise checkboxes.
  {
    ScopedListPrefUpdate update(
        profile_.GetPrefs(), ntp_tiles::prefs::kEnterpriseShortcutsPolicyList);
    update->Append(base::DictValue().Set("title", "Missing URL"));
  }

  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, true);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpEnterpriseShortcutsVisible,
                                  false);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpPersonalShortcutsVisible,
                                  false);
  profile_.GetPrefs()->SetInteger(
      kOmniboxEverywhereShowShortcuts,
      std::to_underlying(ShowShortcutsPrefValue::kUnset));

  EXPECT_TRUE(AreShortcutsAvailableForProfile(&profile_));
  EXPECT_TRUE(IsOmniboxEverywhereShortcutsVisible(&profile_));
}

TEST_F(OmniboxEverywherePrefsTest, SetOmniboxEverywhereHotkeyEnablesHotkey) {
  local_state_.SetBoolean(kHotkeyEnabled, false);
  EXPECT_FALSE(HasOmniboxEverywhereHotkey(&local_state_));

  SetOmniboxEverywhereHotkey(&local_state_, "Ctrl+Shift+Space");
  EXPECT_TRUE(local_state_.GetBoolean(kHotkeyEnabled));
  EXPECT_TRUE(HasOmniboxEverywhereHotkey(&local_state_));
}

TEST_F(OmniboxEverywherePrefsTest, HotkeyPresetsAndTokens) {
  // Without an explicit hotkey, GetOmniboxEverywhereHotkey returns empty.
  EXPECT_TRUE(GetOmniboxEverywhereHotkey(&local_state_).IsEmpty());
  EXPECT_FALSE(HasOmniboxEverywhereHotkey(&local_state_));

  ui::Accelerator alt_space(ui::VKEY_SPACE, ui::EF_ALT_DOWN);
  SetOmniboxEverywhereHotkey(&local_state_, "Alt+Space");
  EXPECT_EQ(GetOmniboxEverywhereHotkey(&local_state_), alt_space);
  EXPECT_TRUE(HasOmniboxEverywhereHotkey(&local_state_));

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

  auto tokens = GetOmniboxEverywhereHotkeyTokens(alt_space);
  EXPECT_FALSE(tokens.empty());
  EXPECT_EQ(tokens.back(), "Space");
#if BUILDFLAG(IS_MAC)
  EXPECT_EQ(tokens[0], "⌥ Option");

  // Verify Apple canonical order [control -> option -> shift -> command].
  ui::Accelerator cmd_shift_space(ui::VKEY_SPACE,
                                  ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN);
  auto cmd_tokens = GetOmniboxEverywhereHotkeyTokens(cmd_shift_space);
  ASSERT_EQ(3u, cmd_tokens.size());
  EXPECT_EQ("⇧ Shift", cmd_tokens[0]);
  EXPECT_EQ("⌘ Cmd", cmd_tokens[1]);
  EXPECT_EQ("Space", cmd_tokens[2]);

  ui::Accelerator ctrl_shift_space(ui::VKEY_SPACE,
                                   ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  auto ctrl_tokens = GetOmniboxEverywhereHotkeyTokens(ctrl_shift_space);
  ASSERT_EQ(3u, ctrl_tokens.size());
  EXPECT_EQ("⌃ Ctrl", ctrl_tokens[0]);
  EXPECT_EQ("⇧ Shift", ctrl_tokens[1]);
  EXPECT_EQ("Space", ctrl_tokens[2]);

  // Verify non-letter keys (Return) map to localized text instead of glyphs.
  ui::Accelerator cmd_return(ui::VKEY_RETURN, ui::EF_COMMAND_DOWN);
  auto return_tokens = GetOmniboxEverywhereHotkeyTokens(cmd_return);
  ASSERT_EQ(2u, return_tokens.size());
  EXPECT_EQ("⌘ Cmd", return_tokens[0]);
  EXPECT_EQ("Enter", return_tokens[1]);
#else
  EXPECT_EQ(tokens[0], "Alt");
#endif
}

TEST_F(OmniboxEverywherePrefsTest,
       ScreenshotDisclosureAccepted_DefaultsToFalse) {
  EXPECT_FALSE(profile_.GetPrefs()->GetBoolean(kScreenshotDisclosureAccepted));
  EXPECT_FALSE(IsScreenshotDisclosureAccepted(&profile_));
  EXPECT_FALSE(IsScreenshotDisclosureAccepted(profile_.GetPrefs()));
  EXPECT_FALSE(
      IsScreenshotDisclosureAccepted(static_cast<const Profile*>(nullptr)));
  EXPECT_FALSE(
      IsScreenshotDisclosureAccepted(static_cast<const PrefService*>(nullptr)));

  SetScreenshotDisclosureAccepted(&profile_, true);
  EXPECT_TRUE(profile_.GetPrefs()->GetBoolean(kScreenshotDisclosureAccepted));
  EXPECT_TRUE(IsScreenshotDisclosureAccepted(&profile_));
  EXPECT_TRUE(IsScreenshotDisclosureAccepted(profile_.GetPrefs()));

  SetScreenshotDisclosureAccepted(profile_.GetPrefs(), false);
  EXPECT_FALSE(profile_.GetPrefs()->GetBoolean(kScreenshotDisclosureAccepted));
  EXPECT_FALSE(IsScreenshotDisclosureAccepted(&profile_));
  EXPECT_FALSE(IsScreenshotDisclosureAccepted(profile_.GetPrefs()));

  // Null safety checks for setters:
  SetScreenshotDisclosureAccepted(static_cast<Profile*>(nullptr), true);
  SetScreenshotDisclosureAccepted(static_cast<PrefService*>(nullptr), true);
}

TEST_F(OmniboxEverywherePrefsTest, ResetProfilePrefs_NullProfileDoesNotCrash) {
  EXPECT_NO_FATAL_FAILURE(ResetProfilePrefs(nullptr));
}

TEST_F(OmniboxEverywherePrefsTest, ResetProfilePrefs) {
  profile_.GetPrefs()->SetInteger(
      kOmniboxEverywhereShowShortcuts,
      std::to_underlying(ShowShortcutsPrefValue::kDisabled));
  profile_.GetPrefs()->SetBoolean(kOmniboxEverywhereShowAiMode, false);
  profile_.GetPrefs()->SetBoolean(kFreDismissed, true);
  profile_.GetPrefs()->SetInteger(kFreImpressionCount, 5);
  profile_.GetPrefs()->SetBoolean(kFreIntroDismissed, true);
  profile_.GetPrefs()->SetInteger(kFreIntroImpressionCount, 2);
  profile_.GetPrefs()->SetBoolean(kFreShortcutSetupDismissed, true);
  profile_.GetPrefs()->SetInteger(kFreShortcutSetupImpressionCount, 3);
  profile_.GetPrefs()->SetBoolean(kFreShortcutReminderDismissed, true);
  profile_.GetPrefs()->SetInteger(kFreShortcutReminderImpressionCount, 3);
  profile_.GetPrefs()->SetBoolean(kScreenshotDisclosureAccepted, true);

  ResetProfilePrefs(&profile_);

  EXPECT_EQ(std::to_underlying(ShowShortcutsPrefValue::kUnset),
            profile_.GetPrefs()->GetInteger(kOmniboxEverywhereShowShortcuts));
  EXPECT_TRUE(profile_.GetPrefs()->GetBoolean(kOmniboxEverywhereShowAiMode));
  EXPECT_FALSE(profile_.GetPrefs()->GetBoolean(kFreDismissed));
  EXPECT_EQ(0, profile_.GetPrefs()->GetInteger(kFreImpressionCount));
  EXPECT_FALSE(profile_.GetPrefs()->GetBoolean(kFreIntroDismissed));
  EXPECT_EQ(0, profile_.GetPrefs()->GetInteger(kFreIntroImpressionCount));
  EXPECT_FALSE(profile_.GetPrefs()->GetBoolean(kFreShortcutSetupDismissed));
  EXPECT_EQ(0,
            profile_.GetPrefs()->GetInteger(kFreShortcutSetupImpressionCount));
  EXPECT_FALSE(profile_.GetPrefs()->GetBoolean(kFreShortcutReminderDismissed));
  EXPECT_EQ(
      0, profile_.GetPrefs()->GetInteger(kFreShortcutReminderImpressionCount));
  EXPECT_FALSE(profile_.GetPrefs()->GetBoolean(kScreenshotDisclosureAccepted));
}

TEST_F(OmniboxEverywherePrefsTest,
       ResetLocalStatePrefs_NullLocalStateDoesNotCrash) {
  EXPECT_NO_FATAL_FAILURE(ResetLocalStatePrefs(nullptr));
}

TEST_F(OmniboxEverywherePrefsTest, ResetLocalStatePrefs) {
  const ui::Accelerator custom_hotkey(ui::VKEY_SPACE,
                                      ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

  local_state_.SetBoolean(kOmniboxEverywhereEnabled, false);
  local_state_.SetBoolean(kHotkeyEnabled, false);
  local_state_.SetString(kOmniboxEverywhereHotkey,
                         ui::Command::AcceleratorToString(custom_hotkey));
  local_state_.SetBoolean(kOmniboxEverywhereBackgroundMode, true);
  local_state_.SetBoolean(kOmniboxEverywhereLaunchOnStartup, true);
  local_state_.SetBoolean(kOmniboxEverywhereEphemeralModel, true);
  local_state_.SetFilePath(kLastTargetProfileDir,
                           base::FilePath(FILE_PATH_LITERAL("test_dir")));

  // Verify custom hotkey before reset.
  EXPECT_EQ(custom_hotkey, GetOmniboxEverywhereHotkey(&local_state_));

  ResetLocalStatePrefs(&local_state_);

  EXPECT_TRUE(local_state_.GetBoolean(kOmniboxEverywhereEnabled));
  EXPECT_TRUE(local_state_.GetBoolean(kHotkeyEnabled));
  EXPECT_TRUE(local_state_.GetString(kOmniboxEverywhereHotkey).empty());
  EXPECT_TRUE(GetOmniboxEverywhereHotkey(&local_state_).IsEmpty());
  EXPECT_FALSE(local_state_.GetBoolean(kOmniboxEverywhereBackgroundMode));
  EXPECT_FALSE(local_state_.GetBoolean(kOmniboxEverywhereLaunchOnStartup));
#if BUILDFLAG(IS_MAC)
  EXPECT_TRUE(local_state_.GetBoolean(kOmniboxEverywhereEphemeralModel));
#else
  EXPECT_FALSE(local_state_.GetBoolean(kOmniboxEverywhereEphemeralModel));
#endif
  EXPECT_EQ(base::FilePath(), local_state_.GetFilePath(kLastTargetProfileDir));
}

}  // namespace
}  // namespace omnibox_everywhere::prefs
