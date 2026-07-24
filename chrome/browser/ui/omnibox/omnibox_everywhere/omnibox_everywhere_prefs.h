// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_PREFS_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_PREFS_H_

class PrefRegistrySimple;

namespace omnibox_everywhere::prefs {

// Boolean preference specifying whether the global hotkey for Omnibox
// Everywhere is enabled.
inline constexpr char kHotkeyEnabled[] = "omnibox_everywhere.hotkey_enabled";

// Boolean preference specifying whether Omnibox Everywhere background mode
// and status tray icon are enabled.
inline constexpr char kOmniboxEverywhereBackgroundMode[] =
    "omnibox_everywhere.background_mode";

// Registers Local State preferences for Omnibox Everywhere.
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

}  // namespace omnibox_everywhere::prefs

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_PREFS_H_
