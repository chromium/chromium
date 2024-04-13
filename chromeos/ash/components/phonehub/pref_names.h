// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_PREF_NAMES_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_PREF_NAMES_H_

namespace ash {
namespace phonehub {
namespace prefs {

extern const char kCameraRollAccessStatus[];
extern const char kNotificationAccessStatus[];
extern const char kNotificationAccessProhibitedReason[];
extern const char kHideOnboardingUi[];
extern const char kIsAwaitingVerifiedHost[];
extern const char kHasDismissedSetupRequiredUi[];
extern const char kNeedsOneTimeNotificationAccessUpdate[];
extern const char kScreenLockStatus[];
extern const char kRecentAppsHistory[];
extern const char kFeatureSetupRequestSupported[];

// Connected phone information used by Phone Hub Structured Metrics
extern const char kPhoneManufacturer[];
extern const char kPhoneModel[];
extern const char kPhoneLocale[];
extern const char kPhonePseudonymousId[];
extern const char kPhoneAndroidVersion[];
extern const char kPhoneGmsCoreVersion[];
extern const char kPhoneAmbientApkVersion[];
extern const char kPhoneProfileType[];
extern const char kPhoneInfoLastUpdatedTime[];
extern const char kChromebookPseudonymousId[];
extern const char kPseudonymousIdRotationDate[];

}  // namespace prefs
}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_PREF_NAMES_H_
