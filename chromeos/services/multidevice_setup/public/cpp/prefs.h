// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_PREFS_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_PREFS_H_

#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

class PrefRegistrySimple;
class PrefService;

namespace chromeos {

namespace multidevice_setup {

// Preferences which represent whether features are allowed by user policy. A
// "false" value means that the administrator has prohibited the feature and
// that users do not have the option of attempting to enable the feature.
extern const char kInstantTetheringAllowedPrefName[];
extern const char kMessagesAllowedPrefName[];
extern const char kSmartLockAllowedPrefName[];
extern const char kSmartLockSigninAllowedPrefName[];

// Preferences which represent whether features are enabled by the user via
// settings. If a feature is prohibited (see above preferences), the "enabled"
// preferences are ignored since the feature is not usable.
extern const char kBetterTogetherSuiteEnabledPrefName[];
extern const char kInstantTetheringEnabledPrefName[];
extern const char kMessagesEnabledPrefName[];
extern const char kSmartLockEnabledPrefName[];

// The old pref which controlled if Smart Lock was enabled, prior to the
// introduction of MultiDeviceSetupService. It will be removed once old Smart
// Lock code is fully deprecated.
extern const char kSmartLockEnabledDeprecatedPrefName[];

void RegisterFeaturePrefs(PrefRegistrySimple* registry);
bool AreAnyMultiDeviceFeaturesAllowed(PrefService* pref_service);
bool IsFeatureAllowed(mojom::Feature feature, PrefService* pref_service);

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_PREFS_H_
