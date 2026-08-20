// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/command.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace omnibox_everywhere {
namespace prefs {

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kHotkeyEnabled, true);
  registry->RegisterStringPref(kOmniboxEverywhereHotkey, "");
  registry->RegisterIntegerPref(
      kOmniboxEverywhereShowShortcuts,
      static_cast<int>(ShowShortcutsPrefValue::kUnset));
  // TODO(crbug.com/543458988): Update default value to reflect enablement /
  // eligibility on rollout.
  registry->RegisterBooleanPref(kOmniboxEverywhereEnabled, false);
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

}  // namespace prefs
}  // namespace omnibox_everywhere
