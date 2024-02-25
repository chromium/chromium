// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/safety_hub_prefs.h"

#include "components/prefs/pref_registry_simple.h"

void RegisterSafetyHubProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(
      safety_hub_prefs::kBackgroundPasswordCheckTimeAndInterval);
  registry->RegisterDictionaryPref(
      safety_hub_prefs::kMenuNotificationsPrefsKey);
}
