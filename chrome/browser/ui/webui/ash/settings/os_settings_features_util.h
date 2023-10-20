// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_OS_SETTINGS_FEATURES_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_OS_SETTINGS_FEATURES_UTIL_H_

class Profile;

namespace ash::settings {

bool IsGuestModeActive();

bool IsChildUser();

bool IsDeviceEnterpriseManaged();

// Determines whether powerwash is allowed for this user. Powerwash is disabled
// for guest users, child users, and managed users.
bool IsPowerwashAllowed();

// Determines whether the Parental Controls section of People settings should be
// shown for `profile`.
bool ShouldShowParentalControlSettings(const Profile* profile);

// Determines whether Android External Storage is enabled for `profile`.
bool IsExternalStorageEnabled(const Profile* profile);

// Determines if app restore settings are available for `profile`.
bool IsAppRestoreAvailableForProfile(const Profile* profile);

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_OS_SETTINGS_FEATURES_UTIL_H_
