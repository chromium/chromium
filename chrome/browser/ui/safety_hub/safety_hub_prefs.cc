// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/safety_hub_prefs.h"

#include "build/build_config.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

void RegisterSafetyHubProfilePrefs(PrefRegistrySimple* registry) {
#if !BUILDFLAG(IS_ANDROID)
  registry->RegisterDictionaryPref(
      safety_hub_prefs::kBackgroundPasswordCheckTimeAndInterval);
#else  // BUILDFLAG(IS_ANDROID)
  // TODO(sideyilmaz): Move kBreachedCredentialsCount to safety_hub_prefs.h
  registry->RegisterIntegerPref(prefs::kBreachedCredentialsCount, -1);
#endif
  registry->RegisterDictionaryPref(
      safety_hub_prefs::kMenuNotificationsPrefsKey);
  registry->RegisterBooleanPref(
      safety_hub_prefs::kUnusedSitePermissionsRevocationEnabled, true);
  registry->RegisterBooleanPref(
      safety_hub_prefs::kUnusedSitePermissionsRevocationMigrationCompleted,
      false);
}
