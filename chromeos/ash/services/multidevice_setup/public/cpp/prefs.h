// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_PREFS_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_PREFS_H_

#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {

namespace multidevice_setup {

enum class EcheSupportReceivedFromPhoneHub {
  // Phone has not specified nor returned a value. Most likely due to the phone
  // running an old version that does not supply the value.
  kNotSpecified = 0,

  kNotSupported = 1,
  kSupported = 2
};

// Preferences which represent whether features are allowed by user policy. A
// "false" value means that the administrator has prohibited the feature and
// that users do not have the option of attempting to enable the feature.
extern const char kInstantTetheringAllowedPrefName[];
extern const char kMessagesAllowedPrefName[];
extern const char kSmartLockAllowedPrefName[];
extern const char kPhoneHubAllowedPrefName[];
extern const char kPhoneHubCameraRollAllowedPrefName[];
extern const char kPhoneHubNotificationsAllowedPrefName[];
extern const char kPhoneHubTaskContinuationAllowedPrefName[];
extern const char kWifiSyncAllowedPrefName[];
extern const char kEcheAllowedPrefName[];

// Preferences which represent whether features are enabled by the user via
// settings. If a feature is prohibited (see above preferences), the "enabled"
// preferences are ignored since the feature is not usable.
extern const char kBetterTogetherSuiteEnabledPrefName[];
extern const char kInstantTetheringEnabledPrefName[];
extern const char kMessagesEnabledPrefName[];
extern const char kSmartLockEnabledPrefName[];
extern const char kPhoneHubEnabledPrefName[];
extern const char kPhoneHubCameraRollEnabledPrefName[];
extern const char kPhoneHubNotificationsEnabledPrefName[];
extern const char kPhoneHubTaskContinuationEnabledPrefName[];
extern const char kEcheEnabledPrefName[];

// Persists the Eche feature support last received from Phone Hub. Note that
// in contrast to server-provided values, this is received over an active
// secure connection, and is therefore more accurate.
extern const char kEcheOverriddenSupportReceivedFromPhoneHubPrefName[];

// The old pref which controlled if Smart Lock was enabled, prior to the
// introduction of MultiDeviceSetupService. It will be removed once old Smart
// Lock code is fully deprecated.
extern const char kSmartLockEnabledDeprecatedPrefName[];

void RegisterFeaturePrefs(PrefRegistrySimple* registry);
bool AreAnyMultiDeviceFeaturesAllowed(const PrefService* pref_service);
bool IsFeatureAllowed(mojom::Feature feature, const PrefService* pref_service);

// Returns true if the pref tracking |feature|'s enabled state is using the
// default value it was registered with.
bool IsDefaultFeatureEnabledValue(mojom::Feature feature,
                                  const PrefService* pref_service);

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_PREFS_H_
