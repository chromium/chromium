// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/pref_names.h"

namespace chromeos {
namespace phonehub {
namespace prefs {

// Whether notification access had been granted by the user on their phone.
const char kNotificationAccessGranted[] =
    "cros.phonehub.notification_access_granted";

// Whether user has completed onboarding and dismissed the UI before.
const char kHasDismissedUiAfterCompletingOnboarding[] =
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
