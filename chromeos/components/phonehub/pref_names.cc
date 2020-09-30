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

}  // namespace prefs
}  // namespace phonehub
}  // namespace chromeos
