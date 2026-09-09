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
#include "components/ntp_tiles/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
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
  registry->RegisterBooleanPref(kOmniboxEverywhereLaunchOnStartup, false);
#if BUILDFLAG(IS_MAC)
  registry->RegisterBooleanPref(kOmniboxEverywhereEphemeralModel, true);
#else
  registry->RegisterBooleanPref(kOmniboxEverywhereEphemeralModel, false);
#endif
  registry->RegisterFilePathPref(kLastTargetProfileDir, base::FilePath());
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(kOmniboxEverywhereShowAiMode, true);
  registry->RegisterBooleanPref(kFreDismissed, false);
  registry->RegisterIntegerPref(kFreImpressionCount, 0);
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

bool AreShortcutsAvailableForProfile(Profile* profile) {
  if (!profile || !profile->GetPrefs()) {
    return true;
  }
  PrefService* prefs = profile->GetPrefs();
  const bool has_enterprise_shortcuts =
      !prefs->GetList(ntp_tiles::prefs::kEnterpriseShortcutsPolicyList).empty();
  if (!has_enterprise_shortcuts) {
    return true;
  }
  const bool enterprise_visible =
      prefs->GetBoolean(ntp_prefs::kNtpEnterpriseShortcutsVisible);
  const bool personal_visible =
      prefs->GetBoolean(ntp_prefs::kNtpPersonalShortcutsVisible);
  return enterprise_visible || personal_visible;
}

bool IsOmniboxEverywhereShortcutsVisible(Profile* profile,
                                         PrefService* local_state) {
  if (local_state) {
    const auto pref_value = static_cast<ShowShortcutsPrefValue>(
        local_state->GetInteger(kOmniboxEverywhereShowShortcuts));
    if (pref_value == ShowShortcutsPrefValue::kDisabled) {
      return false;
    }
    if (pref_value == ShowShortcutsPrefValue::kEnabled) {
      return AreShortcutsAvailableForProfile(profile);
    }
  }

  // Fallback to Customize Chrome / NTP setting.
  if (!profile || !profile->GetPrefs() ||
      !profile->GetPrefs()->GetBoolean(ntp_prefs::kNtpShortcutsVisible)) {
    return false;
  }

  return AreShortcutsAvailableForProfile(profile);
}

}  // namespace prefs
}  // namespace omnibox_everywhere
