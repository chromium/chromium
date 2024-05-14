// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/public/cpp/prefs.h"

#include "ash/constants/ash_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace multidevice_setup {

// Note: Pref name strings follow an inconsistent naming convention because some
// of them were created before the MultiDeviceSetup project.

// "Allowed by user policy" preferences:
const char kInstantTetheringAllowedPrefName[] = "tether.allowed";
const char kMessagesAllowedPrefName[] = "multidevice.sms_connect_allowed";
const char kSmartLockAllowedPrefName[] = "easy_unlock.allowed";
const char kPhoneHubAllowedPrefName[] = "phone_hub.allowed";
const char kPhoneHubCameraRollAllowedPrefName[] =
    "phone_hub_camera_roll.allowed";
const char kPhoneHubNotificationsAllowedPrefName[] =
    "phone_hub_notifications.allowed";
const char kPhoneHubTaskContinuationAllowedPrefName[] =
    "phone_hub_task_continuation.allowed";
const char kWifiSyncAllowedPrefName[] = "wifi_sync.allowed";
const char kEcheAllowedPrefName[] = "eche.allowed";

// "Enabled by user" preferences:
const char kBetterTogetherSuiteEnabledPrefName[] =
    "multidevice_setup.suite_enabled";
const char kInstantTetheringEnabledPrefName[] = "tether.enabled";
const char kMessagesEnabledPrefName[] = "multidevice.sms_connect_enabled";
const char kSmartLockEnabledPrefName[] = "smart_lock.enabled";
const char kSmartLockEnabledDeprecatedPrefName[] = "easy_unlock.enabled";
const char kPhoneHubEnabledPrefName[] = "phone_hub.enabled";
const char kPhoneHubCameraRollEnabledPrefName[] =
    "phone_hub_camera_roll.enabled";
const char kPhoneHubNotificationsEnabledPrefName[] =
    "phone_hub_notifications.enabled";
const char kPhoneHubTaskContinuationEnabledPrefName[] =
    "phone_hub_task_continuation.enabled";
const char kEcheEnabledPrefName[] = "eche.enabled";

const char kEcheOverriddenSupportReceivedFromPhoneHubPrefName[] =
    "eche.overridden_support_received_from_phone_hub";

void RegisterFeaturePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kInstantTetheringAllowedPrefName, true);
  registry->RegisterBooleanPref(kMessagesAllowedPrefName, true);
  registry->RegisterBooleanPref(kSmartLockAllowedPrefName, true);
  registry->RegisterBooleanPref(kPhoneHubAllowedPrefName, true);
  registry->RegisterBooleanPref(kPhoneHubCameraRollAllowedPrefName, true);
  registry->RegisterBooleanPref(kPhoneHubNotificationsAllowedPrefName, true);
  registry->RegisterBooleanPref(kPhoneHubTaskContinuationAllowedPrefName, true);
  registry->RegisterBooleanPref(kWifiSyncAllowedPrefName, true);
  registry->RegisterBooleanPref(kEcheAllowedPrefName, true);

  registry->RegisterBooleanPref(kBetterTogetherSuiteEnabledPrefName, true);
  registry->RegisterBooleanPref(kInstantTetheringEnabledPrefName, true);
  registry->RegisterBooleanPref(kMessagesEnabledPrefName, true);
  registry->RegisterBooleanPref(kSmartLockEnabledDeprecatedPrefName, true);
  registry->RegisterBooleanPref(kSmartLockEnabledPrefName, true);
  registry->RegisterBooleanPref(kEcheEnabledPrefName, false);

  // This pref should be disabled for existing Better Together users;
  // they must go to settings to explicitly enable PhoneHub.
  registry->RegisterBooleanPref(kPhoneHubEnabledPrefName, false);

  registry->RegisterBooleanPref(kPhoneHubCameraRollEnabledPrefName, false);

  // This pref is disabled by default; it should not be enabled until access is
  // granted from the phone.
  registry->RegisterBooleanPref(kPhoneHubNotificationsEnabledPrefName, false);

  registry->RegisterBooleanPref(kPhoneHubTaskContinuationEnabledPrefName, true);

  registry->RegisterIntegerPref(
      kEcheOverriddenSupportReceivedFromPhoneHubPrefName,
      static_cast<int>(EcheSupportReceivedFromPhoneHub::kNotSpecified));
}

bool AreAnyMultiDeviceFeaturesAllowed(const PrefService* pref_service) {
  // There is no policy for the multi-device suite as a whole; instead, the
  // suite is allowed if any available subfeature is allowed.
  return IsFeatureAllowed(mojom::Feature::kBetterTogetherSuite, pref_service);
}

bool IsFeatureAllowed(mojom::Feature feature, const PrefService* pref_service) {
  switch (feature) {
    case mojom::Feature::kBetterTogetherSuite: {
      // Note: Does not check sub-features of Phone Hub, since if the top-level
      // Phone Hub feature is prohibited, its sub-features are implicitly
      // prohibited.
      static const mojom::Feature kTopLevelFeaturesInSuite[] = {
          mojom::Feature::kInstantTethering, mojom::Feature::kMessages,
          mojom::Feature::kPhoneHub,         mojom::Feature::kSmartLock,
          mojom::Feature::kWifiSync,
      };
      for (mojom::Feature top_level_feature : kTopLevelFeaturesInSuite) {
        if (IsFeatureAllowed(top_level_feature, pref_service))
          return true;
      }
      return false;
    }

    case mojom::Feature::kInstantTethering:
      return base::FeatureList::IsEnabled(features::kInstantTethering) &&
             pref_service->GetBoolean(kInstantTetheringAllowedPrefName);

    case mojom::Feature::kMessages:
      return pref_service->GetBoolean(kMessagesAllowedPrefName);

    case mojom::Feature::kSmartLock:
      return pref_service->GetBoolean(kSmartLockAllowedPrefName);

    case mojom::Feature::kPhoneHub:
      return features::IsPhoneHubEnabled() &&
             pref_service->GetBoolean(kPhoneHubAllowedPrefName);

    case mojom::Feature::kPhoneHubCameraRoll:
      return features::IsPhoneHubEnabled() &&
             features::IsPhoneHubCameraRollEnabled() &&
             pref_service->GetBoolean(kPhoneHubCameraRollAllowedPrefName);

    case mojom::Feature::kPhoneHubNotifications:
      return features::IsPhoneHubEnabled() &&
             pref_service->GetBoolean(kPhoneHubNotificationsAllowedPrefName);

    case mojom::Feature::kPhoneHubTaskContinuation:
      return features::IsPhoneHubEnabled() &&
             pref_service->GetBoolean(kPhoneHubTaskContinuationAllowedPrefName);

    case mojom::Feature::kWifiSync:
      return features::IsWifiSyncAndroidEnabled() &&
             pref_service->GetBoolean(kWifiSyncAllowedPrefName);

    case mojom::Feature::kEche:
      return features::IsEcheSWAEnabled() &&
             pref_service->GetBoolean(kEcheAllowedPrefName);

    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

bool IsDefaultFeatureEnabledValue(mojom::Feature feature,
                                  const PrefService* pref_service) {
  switch (feature) {
    case mojom::Feature::kBetterTogetherSuite:
      return pref_service->FindPreference(kBetterTogetherSuiteEnabledPrefName)
          ->IsDefaultValue();
    case mojom::Feature::kInstantTethering:
      return pref_service->FindPreference(kInstantTetheringEnabledPrefName)
          ->IsDefaultValue();
    case mojom::Feature::kMessages:
      return pref_service->FindPreference(kMessagesEnabledPrefName)
          ->IsDefaultValue();
    case mojom::Feature::kSmartLock:
      return pref_service->FindPreference(kSmartLockEnabledPrefName)
          ->IsDefaultValue();
    case mojom::Feature::kPhoneHub:
      return pref_service->FindPreference(kPhoneHubEnabledPrefName)
          ->IsDefaultValue();
    case mojom::Feature::kPhoneHubCameraRoll:
      return pref_service->FindPreference(kPhoneHubCameraRollEnabledPrefName)
          ->IsDefaultValue();
    case mojom::Feature::kPhoneHubNotifications:
      return pref_service->FindPreference(kPhoneHubNotificationsEnabledPrefName)
          ->IsDefaultValue();
    case mojom::Feature::kPhoneHubTaskContinuation:
      return pref_service
          ->FindPreference(kPhoneHubTaskContinuationEnabledPrefName)
          ->IsDefaultValue();
    case mojom::Feature::kWifiSync:
      NOTREACHED_IN_MIGRATION();
      return false;
    case mojom::Feature::kEche:
      return pref_service->FindPreference(kEcheEnabledPrefName)
          ->IsDefaultValue();
  }
}

}  // namespace multidevice_setup

}  // namespace ash
