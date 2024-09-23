// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/language_preferences/language_preferences.h"
#include "components/prefs/pref_registry_simple.h"

namespace ash {
namespace language_prefs {

// ---------------------------------------------------------------------------
// For ibus-daemon
// ---------------------------------------------------------------------------
const char kGeneralSectionName[] = "general";
const char kPreloadEnginesConfigName[] = "preload_engines";

// ---------------------------------------------------------------------------
// For keyboard stuff
// ---------------------------------------------------------------------------
const char kPreferredKeyboardLayout[] = "PreferredKeyboardLayout";

void RegisterPrefs(PrefRegistrySimple* registry) {
  // We use an empty string here rather than a hardware keyboard layout name
  // since input_method::GetHardwareInputMethodId() might return a fallback
  // layout name if registry->RegisterStringPref(kHardwareKeyboardLayout)
  // is not called yet.
  registry->RegisterStringPref(kPreferredKeyboardLayout, "");
}

}  // namespace language_prefs
}  // namespace ash
