// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKTOP_TO_MOBILE_PROMOS_PREF_NAMES_H_
#define COMPONENTS_DESKTOP_TO_MOBILE_PROMOS_PREF_NAMES_H_

namespace prefs {

// Dictionary that stores information about the promo reminder to be shown on an
// iOS device. It contains the promo type and the target device GUID.
inline constexpr char kIOSPromoReminder[] = "promos.ios_promo_reminder";

// Keys for the kIOSPromoReminder dictionary.
inline constexpr char kIOSPromoReminderPromoType[] = "promo_type";
inline constexpr char kIOSPromoReminderDeviceGUID[] = "device_guid";
inline constexpr char kIOSPromoReminderTriggerId[] = "trigger_id";

// String pref for storing a trigger ID that has already been
// shown on iOS.
inline constexpr char kCrossPlatformPromosConsumedTriggerId[] =
    "cross_platform_promos.consumed_trigger_id";

}  // namespace prefs

#endif  // COMPONENTS_DESKTOP_TO_MOBILE_PROMOS_PREF_NAMES_H_
