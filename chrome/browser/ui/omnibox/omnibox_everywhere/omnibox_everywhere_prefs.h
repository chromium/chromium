// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_PREFS_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_PREFS_H_

class PrefRegistrySimple;
class PrefService;

namespace ui {
class Accelerator;
}

namespace omnibox_everywhere {
namespace prefs {

// Boolean preference specifying whether the global hotkey for Omnibox
// Everywhere is enabled.
inline constexpr char kHotkeyEnabled[] = "omnibox_everywhere.hotkey_enabled";

// String preference storing custom global hotkey combination for Omnibox
// Everywhere.
inline constexpr char kOmniboxEverywhereHotkey[] = "omnibox_everywhere.hotkey";

// Tri-state value specifying whether shortcuts are shown in Omnibox Everywhere.
enum class ShowShortcutsPrefValue {
  kUnset = 0,     // Fallback to Customize Chrome / NTP setting.
  kDisabled = 1,  // Explicitly disabled in Omnibox Everywhere.
  kEnabled = 2,   // Explicitly enabled in Omnibox Everywhere.
};

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

// Boolean preference specifying whether Omnibox Everywhere uses the ephemeral
// (close/hide on focus loss) model instead of the persistent model.
inline constexpr char kOmniboxEverywhereEphemeralModel[] =
    "omnibox_everywhere.ephemeral_model";
// FilePath preference specifying the path of the last target profile set
// for Omnibox Everywhere.
inline constexpr char kLastTargetProfileDir[] =
    "omnibox_everywhere.last_target_profile_dir";

// Registers Local State preferences for Omnibox Everywhere.
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

// Returns the default global hotkey accelerator for Omnibox Everywhere.
ui::Accelerator GetDefaultOmniboxEverywhereHotkey();

// Returns the configured global hotkey accelerator for Omnibox Everywhere from
// local state, falling back to the default accelerator if unset or invalid.
ui::Accelerator GetOmniboxEverywhereHotkey(PrefService* local_state);

}  // namespace prefs
}  // namespace omnibox_everywhere

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_PREFS_H_
