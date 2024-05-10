// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_OS_SETTINGS_FEATURES_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_OS_SETTINGS_FEATURES_UTIL_H_

class Profile;

namespace ash::settings {

bool IsGuestModeActive();

bool IsChildUser();

// Determines whether powerwash is allowed for this user. Powerwash is disabled
// for guest users, child users, and managed users.
bool IsPowerwashAllowed();

// Determines whether Sanitize is allowed for the user. Managed users, guest
// users, and child users cannot use the sanitize feature. Also Sanitize is
// initially only enabled through a flag.
bool IsSanitizeAllowed();

// Determines whether the Parental Controls section of People settings should be
// shown for `profile`.
bool ShouldShowParentalControlSettings(const Profile* profile);

// Determines whether Android External Storage is enabled for `profile`.
bool IsExternalStorageEnabled(const Profile* profile);

// Determines if app restore settings are available for `profile`.
bool IsAppRestoreAvailableForProfile(const Profile* profile);

// Determines if per-app language settings are available for `profile`.
bool IsPerAppLanguageEnabled(const Profile* profile);

// Determines if multitasking section of System Preferences is allowed.
// This function is used to show the window suggestions option in the Settings
// app when the ash feature `kOsSettingsRevampWayfinding` and
// `kFasterSplitScreenSetup` are both enabled.
bool ShouldShowMultitasking();

// Determines if multitasking section is allowed to show as a sub-section in
// personalization section.
// This function is used to show the window suggestions option in the Settings
// app when the ash feature `kOsSettingsRevampWayfinding` is disabled and
// `kFasterSplitScreenSetup` is enabled.
bool ShouldShowMultitaskingInPersonalization();

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_OS_SETTINGS_FEATURES_UTIL_H_
