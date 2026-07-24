// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace omnibox_everywhere::prefs {

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kHotkeyEnabled, true);
  registry->RegisterBooleanPref(kOmniboxEverywhereBackgroundMode, false);
}

}  // namespace omnibox_everywhere::prefs
