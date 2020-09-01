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
const char kPhoneHubAllowedPrefName[] = "phone_hub.allowed";
const char kPhoneHubNotificationsAllowedPrefName[] =
    "phone_hub_notifications.allowed";
const char kPhoneHubTaskContinuationAllowedPrefName[] =
    "phone_hub_task_continuation.allowed";
const char kWifiSyncAllowedPrefName[] = "wifi_sync.allowed";

// "Enabled by user" preferences:
const char kBetterTogetherSuiteEnabledPrefName[] =
    "multidevice_setup.suite_enabled";
const char kInstantTetheringEnabledPrefName[] = "tether.enabled";
const char kMessagesEnabledPrefName[] = "multidevice.sms_connect_enabled";
const char kSmartLockEnabledPrefName[] = "smart_lock.enabled";
const char kSmartLockEnabledDeprecatedPrefName[] = "easy_unlock.enabled";
const char kPhoneHubEnabledPrefName[] = "phone_hub.enabled";
const char kPhoneHubNotificationsEnabledPrefName[] =
    "phone_hub_notifications.enabled";
const char kPhoneHubNotificationBadgeEnabledPrefName[] =
    "phone_hub_notification_badge.enabled";
const char kPhoneHubTaskContinuationEnabledPrefName[] =
    "phone_hub_task_continuation.enabled";

void RegisterFeaturePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kInstantTetheringAllowedPrefName, true);
  registry->RegisterBooleanPref(kMessagesAllowedPrefName, true);
  registry->RegisterBooleanPref(kSmartLockAllowedPrefName, true);
  registry->RegisterBooleanPref(kSmartLockSigninAllowedPrefName, true);
  registry->RegisterBooleanPref(kPhoneHubAllowedPrefName, true);
  registry->RegisterBooleanPref(kPhoneHubNotificationsAllowedPrefName, true);
  registry->RegisterBooleanPref(kPhoneHubTaskContinuationAllowedPrefName, true);
  registry->RegisterBooleanPref(kWifiSyncAllowedPrefName, true);

  registry->RegisterBooleanPref(kBetterTogetherSuiteEnabledPrefName, true);
  registry->RegisterBooleanPref(kInstantTetheringEnabledPrefName, true);
  registry->RegisterBooleanPref(kMessagesEnabledPrefName, true);
  registry->RegisterBooleanPref(kSmartLockEnabledDeprecatedPrefName, true);
  registry->RegisterBooleanPref(kSmartLockEnabledPrefName, true);
  registry->RegisterBooleanPref(kPhoneHubEnabledPrefName, true);
  registry->RegisterBooleanPref(kPhoneHubNotificationsEnabledPrefName, true);
  registry->RegisterBooleanPref(kPhoneHubNotificationBadgeEnabledPrefName,
                                true);
  registry->RegisterBooleanPref(kPhoneHubTaskContinuationEnabledPrefName, true);
}

bool AreAnyMultiDeviceFeaturesAllowed(const PrefService* pref_service) {
  // Note: Does not check sub-features of Phone Hub, since if the top-level
  // Phone Hub feature is prohibited, its sub-features are implicitly
  // prohibited.
  return pref_service->GetBoolean(kInstantTetheringAllowedPrefName) ||
         pref_service->GetBoolean(kMessagesAllowedPrefName) ||
         pref_service->GetBoolean(kSmartLockAllowedPrefName) ||
         pref_service->GetBoolean(kPhoneHubAllowedPrefName) ||
         pref_service->GetBoolean(kWifiSyncAllowedPrefName);
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
    case mojom::Feature::kPhoneHub:
      return pref_service->GetBoolean(kPhoneHubAllowedPrefName);
    case mojom::Feature::kPhoneHubNotifications:
      return pref_service->GetBoolean(kPhoneHubNotificationsAllowedPrefName);
    // Note: Uses the same "allowed" pref for notification usage in general.
    case mojom::Feature::kPhoneHubNotificationBadge:
      return pref_service->GetBoolean(kPhoneHubNotificationsAllowedPrefName);
    case mojom::Feature::kPhoneHubTaskContinuation:
      return pref_service->GetBoolean(kPhoneHubTaskContinuationAllowedPrefName);
    case mojom::Feature::kWifiSync:
      return pref_service->GetBoolean(kWifiSyncAllowedPrefName);
    default:
      NOTREACHED();
      return false;
  }
}

}  // namespace multidevice_setup

}  // namespace chromeos
