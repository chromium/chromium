// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_FEATURES_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_FEATURES_UTIL_H_

class Profile;

namespace chromeos {
namespace settings {
namespace features {

bool IsGuestModeActive();

// Determines whether the Parental Controls section of People settings should be
// shown for |profile|.
bool ShouldShowParentalControlSettings(const Profile* profile);

// Determines whether the External Storage section of Device settings should be
// shown for |profile|.
bool ShouldShowExternalStorageSettings(const Profile* profile);

}  // namespace features
}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_FEATURES_UTIL_H_
