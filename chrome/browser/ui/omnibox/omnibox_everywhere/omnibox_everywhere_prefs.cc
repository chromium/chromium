// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"

#include <utility>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/new_tab_page/prefs/ntp_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/command.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace omnibox_everywhere {
namespace prefs {

bool IsEphemeralModelEnabled() {
  if (g_browser_process && g_browser_process->local_state()) {
    return g_browser_process->local_state()->GetBoolean(
        kOmniboxEverywhereEphemeralModel);
  }
#if BUILDFLAG(IS_MAC)
  return true;
#else
  return false;
#endif
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kHotkeyEnabled, true);
  registry->RegisterStringPref(kOmniboxEverywhereHotkey, "");
  registry->RegisterIntegerPref(
      kOmniboxEverywhereShowShortcuts,
      std::to_underlying(ShowShortcutsPrefValue::kUnset));
  registry->RegisterBooleanPref(kOmniboxEverywhereEnabled, true);
  registry->RegisterBooleanPref(kOmniboxEverywhereBackgroundMode, false);
#if BUILDFLAG(IS_MAC)
  registry->RegisterBooleanPref(kOmniboxEverywhereEphemeralModel, true);
#else
  registry->RegisterBooleanPref(kOmniboxEverywhereEphemeralModel, false);
#endif
  registry->RegisterFilePathPref(kLastTargetProfileDir, base::FilePath());
}

ui::Accelerator GetDefaultOmniboxEverywhereHotkey() {
  return ui::Accelerator(ui::VKEY_SPACE, ui::EF_ALT_DOWN);
}

ui::Accelerator GetOmniboxEverywhereHotkey(PrefService* local_state) {
  if (!local_state) {
    return GetDefaultOmniboxEverywhereHotkey();
  }

  const std::string hotkey_str =
      local_state->GetString(kOmniboxEverywhereHotkey);
  if (hotkey_str.empty()) {
    return GetDefaultOmniboxEverywhereHotkey();
  }

  const ui::Accelerator hotkey = ui::Command::StringToAccelerator(hotkey_str);
  if (!hotkey.IsEmpty() &&
      ui::Accelerator::MaskOutKeyEventFlags(hotkey.modifiers()) != 0) {
    return hotkey;
  }

  return GetDefaultOmniboxEverywhereHotkey();
}

bool IsOmniboxEverywhereShortcutsVisible(Profile* profile,
                                         PrefService* local_state) {
  if (local_state) {
    const auto pref_value = static_cast<ShowShortcutsPrefValue>(
        local_state->GetInteger(kOmniboxEverywhereShowShortcuts));
    if (pref_value == ShowShortcutsPrefValue::kEnabled) {
      return true;
    }
    if (pref_value == ShowShortcutsPrefValue::kDisabled) {
      return false;
    }
  }

  // Fallback to Customize Chrome / NTP setting.
  return profile && profile->GetPrefs() &&
         profile->GetPrefs()->GetBoolean(ntp_prefs::kNtpShortcutsVisible);
}

}  // namespace prefs
}  // namespace omnibox_everywhere
