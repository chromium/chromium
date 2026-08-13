// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "components/prefs/pref_registry_simple.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace omnibox_everywhere {

ui::Accelerator GetHotkey() {
  // TODO(crbug.com/546111112): Add support for customizable hotkey.
  return ui::Accelerator(ui::VKEY_SPACE, ui::EF_ALT_DOWN);
}

namespace prefs {

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kHotkeyEnabled, true);
  registry->RegisterBooleanPref(kOmniboxEverywhereBackgroundMode, false);
#if BUILDFLAG(IS_MAC)
  registry->RegisterBooleanPref(kOmniboxEverywhereEphemeralModel, true);
#else
  registry->RegisterBooleanPref(kOmniboxEverywhereEphemeralModel, false);
#endif
  registry->RegisterFilePathPref(kLastTargetProfileDir, base::FilePath());
}

}  // namespace prefs
}  // namespace omnibox_everywhere
