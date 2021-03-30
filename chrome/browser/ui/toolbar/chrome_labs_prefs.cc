// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/chrome_labs_prefs.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"

namespace chrome_labs_prefs {

// Boolean pref indicating whether Chrome Labs experimental features are enabled
// with toolbar entry point. This pref is mapped to an enterprise policy value.
const char kBrowserLabsEnabled[] = "browser_labs_enabled";

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(kBrowserLabsEnabled, true);
}

}  // namespace chrome_labs_prefs
