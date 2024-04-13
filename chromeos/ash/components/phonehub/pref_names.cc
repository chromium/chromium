// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/pref_names.h"

namespace ash {
namespace phonehub {
namespace prefs {

// The last provided camera roll access status provided by the phone. This pref
// stores the numerical value associated with the
// MultideviceFeatureAccessManager::CameraRollAccessStatus enum.
const char kCameraRollAccessStatus[] =
    "cros.phonehub.camera_roll_access_status";

// The last provided notification access status provided by the phone. This pref
// stores the numerical value associated with the
// MultideviceFeatureAccessManager::AccessStatus enum.
const char kNotificationAccessStatus[] =
    "cros.phonehub.notification_access_status";

// The last provided reason for notification access being prohibited. This pref
// stores the numerical value associated with the
// MultideviceFeatureAccessManager::AccessProhibitedReason enum. This pref may
// be left in an undefined state if notification access is not prohibited.
const char kNotificationAccessProhibitedReason[] =
    "cros.phonehub.notification_access_prohibited_reason";

// Whether user has completed onboarding and dismissed the UI before or if
// the user has already gone through the onboarding process and has enabled the
// feature. Note: The pref name is no longer accurate as there are multiple ways
// the onboarding UI can be hidden. |kHideOnboardingUi| is a generic variable
// name to better convey the functionality of the pref.
const char kHideOnboardingUi[] =
    "cros.phonehub.has_completed_onboarding_before";

// Whether the MultideviceSetupStateUpdater is waiting for a verified host
// in order to enable the Multidevice PhoneHub feature.
const char kIsAwaitingVerifiedHost[] =
    "cros.phonehub.is_awaiting_verified_host";

// Whether the Notification access setup banner in the PhoneHub UI has
// been dismissed.
const char kHasDismissedSetupRequiredUi[] =
    "cros.phonehub.has_dismissed_setup_required_ui";

// TODO(http://crbug.com/1215559): Deprecate when there are no more active Phone
// Hub notification users on M89. Some users had notifications automatically
// disabled when updating from M89 to M90+ because the notification feature
// state went from enabled-by-default to disabled-by-default. To re-enable those
// users, we once and only once notify observers if access has been granted by
// the phone. Notably, the MultideviceSetupStateUpdate will decide whether or
// not the notification feature should be enabled. See
// MultideviceSetupStateUpdater's method
// IsWaitingForAccessToInitiallyEnableNotifications() for more details.
const char kNeedsOneTimeNotificationAccessUpdate[] =
    "cros.phonehub.needs_one_time_notification_access_update";

// The last provided screen lock status provided by the phone. This pref stores
// the numerical value associated with the ScreenLockManager::LockStatus enum.
const char kScreenLockStatus[] = "cros.phonehub.screen_lock_status";

// The last provided recent app information before the Eche disconnects. The
// pref stores the vector value associated with Notification::AppMetadata.
const char kRecentAppsHistory[] = "cros.phonehub.recent_apps_history";

// Whether the phone supports setting up multiple features at the same time
// using the FeatureSetupRequest.
const char kFeatureSetupRequestSupported[] =
    "cros.phonehub.feature_setup_request_supported";

const char kPhoneManufacturer[] = "cros.phonehub.phone_manufacturer";
const char kPhoneModel[] = "cros.phonehub.phone_model";
const char kPhoneLocale[] = "cros.phonehub.phone_locale";
const char kPhonePseudonymousId[] = "cros.phonehub.phone_pseudonymous_id";
const char kPhoneAndroidVersion[] = "cros.phonehub.phone_android_version";
const char kPhoneGmsCoreVersion[] = "cros.phonehub.phone_gms_core_version";
const char kPhoneAmbientApkVersion[] =
    "cros.phonehub.phone_ambient_apk_version";
const char kPhoneProfileType[] = "cros.phonehub.phone_profile_type";
const char kPhoneInfoLastUpdatedTime[] =
    "cros.phonehub.phone_info_last_updated_time";
const char kChromebookPseudonymousId[] =
    "cros.phonehub.chromebook_pseudonymous_id";
const char kPseudonymousIdRotationDate[] =
    "cros.phonehub.pseudonymous_id_rotation_date";

}  // namespace prefs
}  // namespace phonehub
}  // namespace ash
