// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace multidevice_setup {

// Note: Pref name strings follow an inconsistent naming convention because some
// of them were created before the MultiDeviceSetup project.

// "Allowed by user policy" preferences:
const char kInstantTetheringAllowedPrefName[] = "tether.allowed";
const char kMessagesAllowedPrefName[] = "multidevice.sms_connect_allowed";
const char kSmartLockAllowedPrefName[] = "easy_unlock.allowed";
const char kSmartLockSigninAllowedPrefName[] = "smart_lock_signin.allowed";

// "Enabled by user" preferences:
const char kBetterTogetherSuiteEnabledPrefName[] =
    "multidevice_setup.suite_enabled";
const char kInstantTetheringEnabledPrefName[] = "tether.enabled";
const char kMessagesEnabledPrefName[] = "multidevice.sms_connect_enabled";
const char kSmartLockEnabledPrefName[] = "smart_lock.enabled";
const char kSmartLockEnabledDeprecatedPrefName[] = "easy_unlock.enabled";

void RegisterFeaturePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kInstantTetheringAllowedPrefName, true);
  registry->RegisterBooleanPref(kMessagesAllowedPrefName, true);
  registry->RegisterBooleanPref(kSmartLockAllowedPrefName, true);
  registry->RegisterBooleanPref(kSmartLockSigninAllowedPrefName, true);

  registry->RegisterBooleanPref(kBetterTogetherSuiteEnabledPrefName, true);
  registry->RegisterBooleanPref(kInstantTetheringEnabledPrefName, true);
  registry->RegisterBooleanPref(kMessagesEnabledPrefName, true);
  registry->RegisterBooleanPref(kSmartLockEnabledDeprecatedPrefName, true);
  registry->RegisterBooleanPref(kSmartLockEnabledPrefName, true);
}

bool AreAnyMultiDeviceFeaturesAllowed(PrefService* pref_service) {
  return pref_service->GetBoolean(kInstantTetheringAllowedPrefName) ||
         pref_service->GetBoolean(kMessagesAllowedPrefName) ||
         pref_service->GetBoolean(kSmartLockAllowedPrefName);
}

bool IsFeatureAllowed(mojom::Feature feature, PrefService* pref_service) {
  switch (feature) {
    case mojom::Feature::kBetterTogetherSuite:
      return AreAnyMultiDeviceFeaturesAllowed(pref_service);
    case mojom::Feature::kInstantTethering:
      return pref_service->GetBoolean(kInstantTetheringAllowedPrefName);
    case mojom::Feature::kMessages:
      return pref_service->GetBoolean(kMessagesAllowedPrefName);
    case mojom::Feature::kSmartLock:
      return pref_service->GetBoolean(kSmartLockAllowedPrefName);
    default:
      NOTREACHED();
      return false;
  }
}

}  // namespace multidevice_setup

}  // namespace chromeos
