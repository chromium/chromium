// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/pref_names.h"

namespace chromeos {
namespace phonehub {
namespace prefs {

// The last provided notification access status provided by the phone. This pref
// stores the numerical value associated with the
// NotificationAccessManager::AccessStatus enum.
const char kNotificationAccessStatus[] =
    "cros.phonehub.notification_access_status";

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

}  // namespace prefs
}  // namespace phonehub
}  // namespace chromeos
