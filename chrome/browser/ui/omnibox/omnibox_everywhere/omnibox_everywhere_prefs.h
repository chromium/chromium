// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_PREFS_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_PREFS_H_

class PrefRegistrySimple;

namespace ui {
class Accelerator;
}

namespace omnibox_everywhere {

// Returns the global hotkey accelerator for Omnibox Everywhere.
ui::Accelerator GetHotkey();

namespace prefs {

// Boolean preference specifying whether the global hotkey for Omnibox
// Everywhere is enabled.
inline constexpr char kHotkeyEnabled[] = "omnibox_everywhere.hotkey_enabled";

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

}  // namespace prefs
}  // namespace omnibox_everywhere

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_PREFS_H_
