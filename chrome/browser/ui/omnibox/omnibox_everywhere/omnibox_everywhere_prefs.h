// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_PREFS_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_PREFS_H_

class PrefRegistrySimple;
class PrefService;
class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace ui {
class Accelerator;
}

namespace omnibox_everywhere {
namespace prefs {

// Returns true if the ephemeral model (close/hide on focus loss) is enabled.
bool IsEphemeralModelEnabled();

// Boolean preference specifying whether the global hotkey for Omnibox
// Everywhere is enabled.
inline constexpr char kHotkeyEnabled[] = "omnibox_everywhere.hotkey_enabled";

// String preference storing custom global hotkey combination for Omnibox
// Everywhere.
inline constexpr char kOmniboxEverywhereHotkey[] = "omnibox_everywhere.hotkey";

// LINT.IfChange(ShowShortcutsPrefValue)
// Tri-state value specifying whether shortcuts are shown in Omnibox Everywhere.
enum class ShowShortcutsPrefValue {
  kUnset = 0,     // Fallback to Customize Chrome / NTP setting.
  kDisabled = 1,  // Explicitly disabled in Omnibox Everywhere.
  kEnabled = 2,   // Explicitly enabled in Omnibox Everywhere.
};
// LINT.ThenChange(//chrome/browser/resources/settings/search_page/omnibox_everywhere_section.ts:ShowShortcutsPrefValue)

// Integer preference specifying whether shortcuts are shown in Omnibox
// Everywhere. See ShowShortcutsPrefValue for values.
inline constexpr char kOmniboxEverywhereShowShortcuts[] =
    "omnibox_everywhere.show_shortcuts";

// Boolean preference specifying whether Omnibox Everywhere (Search in Chrome)
// is enabled (main settings toggle).
inline constexpr char kOmniboxEverywhereEnabled[] =
    "omnibox_everywhere.enabled";

// Boolean preference specifying whether Omnibox Everywhere background mode
// and status tray icon are enabled.
inline constexpr char kOmniboxEverywhereBackgroundMode[] =
    "omnibox_everywhere.background_mode";

// Boolean preference specifying whether Omnibox Everywhere launches on OS
// startup.
inline constexpr char kOmniboxEverywhereLaunchOnStartup[] =
    "omnibox_everywhere.launch_on_startup";

// Boolean preference specifying whether Omnibox Everywhere uses the ephemeral
// (close/hide on focus loss) model instead of the persistent model.
inline constexpr char kOmniboxEverywhereEphemeralModel[] =
    "omnibox_everywhere.ephemeral_model";

// Boolean preference specifying whether AI Mode / Fusebox entrypoints are
// enabled in Omnibox Everywhere.
inline constexpr char kOmniboxEverywhereShowAiMode[] =
    "omnibox_everywhere.show_ai_mode";

// FilePath preference specifying the path of the last target profile set
// for Omnibox Everywhere.
inline constexpr char kLastTargetProfileDir[] =
    "omnibox_everywhere.last_target_profile_dir";

// Boolean preference specifying whether the First Run Experience (FRE)
// modal for Omnibox Everywhere has been dismissed or completed.
inline constexpr char kFreDismissed[] = "omnibox_everywhere.fre_dismissed";

// Integer preference storing the number of times the First Run Experience (FRE)
// modal has been shown to the user.
inline constexpr char kFreImpressionCount[] =
    "omnibox_everywhere.fre_impression_count";

// Maximum number of impressions the FRE modal will be shown before
// auto-dismissing.
inline constexpr int kMaxFreImpressions = 3;

// Registers Local State preferences for Omnibox Everywhere.
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

// Registers Profile preferences for Omnibox Everywhere.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Returns the default global hotkey accelerator for Omnibox Everywhere.
ui::Accelerator GetDefaultOmniboxEverywhereHotkey();

// Returns the configured global hotkey accelerator for Omnibox Everywhere from
// local state, falling back to the default accelerator if unset or invalid.
ui::Accelerator GetOmniboxEverywhereHotkey(PrefService* local_state);

// Returns whether any shortcuts (enterprise or personal) are available and
// enabled for the profile.
bool AreShortcutsAvailableForProfile(Profile* profile);

// Returns whether shortcuts should be shown in Omnibox Everywhere for the given
// profile and local state, falling back to Customize Chrome / NTP settings
// (kNtpShortcutsVisible) if the Omnibox Everywhere preference is unset.
bool IsOmniboxEverywhereShortcutsVisible(Profile* profile,
                                         PrefService* local_state);

}  // namespace prefs
}  // namespace omnibox_everywhere

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_PREFS_H_
