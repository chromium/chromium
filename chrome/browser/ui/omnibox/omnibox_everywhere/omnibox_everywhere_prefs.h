// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_PREFS_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_PREFS_H_

#include <string>
#include <string_view>
#include <vector>

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

// Stages of the Omnibox Everywhere First Run Experience (FRE).
enum class FreStage {
  // FRE is complete or inactive.
  kNone = 0,
  // Stage 1: Two-row modal card (Value prop + Where to find).
  kIntroModal = 1,
  // Stage 2: One-row chin with shortcut selector dropdown & "Set as shortcut"
  // CTA.
  kShortcutSetupChin = 2,
  // Stage 3: One-row educational chin showing active shortcut reminder.
  kShortcutReminderChin = 3,
};

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
// is enabled (main settings toggle) and status tray icon is shown.
inline constexpr char kOmniboxEverywhereEnabled[] =
    "omnibox_everywhere.enabled";

// Boolean preference specifying whether Omnibox Everywhere background mode
// is enabled.
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

// Boolean preference specifying whether the overall First Run Experience (FRE)
// modal for Omnibox Everywhere has been dismissed or completed.
inline constexpr char kFreDismissed[] = "omnibox_everywhere.fre_dismissed";

// Integer preference storing the legacy number of times the FRE modal was
// shown. Retained for compatibility with pre-multi-stage WebUI controllers.
inline constexpr char kFreImpressionCount[] =
    "omnibox_everywhere.fre_impression_count";

// Preferences for Stage 1: Intro Modal.
inline constexpr char kFreIntroDismissed[] =
    "omnibox_everywhere.fre_intro_dismissed";
inline constexpr char kFreIntroImpressionCount[] =
    "omnibox_everywhere.fre_intro_impression_count";
inline constexpr int kMaxFreIntroImpressions = 2;

// Preferences for Stage 2: Shortcut Setup Chin.
inline constexpr char kFreShortcutSetupDismissed[] =
    "omnibox_everywhere.fre_shortcut_setup_dismissed";
inline constexpr char kFreShortcutSetupImpressionCount[] =
    "omnibox_everywhere.fre_shortcut_setup_impression_count";
inline constexpr int kMaxFreShortcutSetupImpressions = 3;

// Preferences for Stage 3: Shortcut Reminder Chin.
inline constexpr char kFreShortcutReminderDismissed[] =
    "omnibox_everywhere.fre_shortcut_reminder_dismissed";
inline constexpr char kFreShortcutReminderImpressionCount[] =
    "omnibox_everywhere.fre_shortcut_reminder_impression_count";
inline constexpr int kMaxFreShortcutReminderImpressions = 3;

// Legacy maximum number of impressions.
inline constexpr int kMaxFreImpressions = 3;

// Registers Local State preferences for Omnibox Everywhere.
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

// Registers Profile preferences for Omnibox Everywhere.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// TODO(crbug.com/559175507): Introduce a dedicated onboarding controller to
// encapsulate FRE state machine transitions and pref observation.
// Returns the active FRE stage for the profile.
FreStage GetCurrentFreStage(Profile* profile,
                            PrefService* local_state = nullptr);

// Increments the impression count for the currently active FRE stage.
void IncrementFreImpression(Profile* profile,
                            PrefService* local_state = nullptr);

// Records that the specified FRE stage was dismissed (or marks overall FRE
// completed).
void OnFreStageDismissed(Profile* profile,
                         FreStage stage,
                         PrefService* local_state = nullptr);

// Returns the default global hotkey accelerator for Omnibox Everywhere.
ui::Accelerator GetDefaultOmniboxEverywhereHotkey();

// Returns the configured global hotkey accelerator for Omnibox Everywhere from
// local state, falling back to the default accelerator if unset or invalid.
ui::Accelerator GetOmniboxEverywhereHotkey(PrefService* local_state);

// Sets the configured global hotkey accelerator in local state.
void SetOmniboxEverywhereHotkey(PrefService* local_state,
                                std::string_view hotkey_str);

// Returns true if a global hotkey combination is available for Omnibox
// Everywhere (either explicitly configured or default fallback) and enabled.
bool HasOmniboxEverywhereHotkey(PrefService* local_state);

// Returns token strings for the accelerator (e.g. ["Cmd", "Shift", "Space"]).
std::vector<std::string> GetOmniboxEverywhereHotkeyTokens(
    const ui::Accelerator& accelerator);

// Returns available shortcut presets for the current platform.
std::vector<std::string> GetAvailableHotkeyPresets();

// Returns whether any shortcuts (enterprise or personal) are available and
// enabled for the profile.
bool AreShortcutsAvailableForProfile(Profile* profile);

// Returns whether shortcuts should be shown in Omnibox Everywhere for the given
// profile, falling back to Customize Chrome / NTP settings
// (kNtpShortcutsVisible) if the Omnibox Everywhere preference is unset.
bool IsOmniboxEverywhereShortcutsVisible(Profile* profile);

}  // namespace prefs
}  // namespace omnibox_everywhere

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_PREFS_H_
