// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CONSTANTS_ROUTES_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CONSTANTS_ROUTES_UTIL_H_

#include <string>

namespace chromeos::settings {

// Returns true if the sub-page is one of the above.
bool IsOSSettingsSubPage(const std::string& sub_page);

}  // namespace chromeos::settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CONSTANTS_ROUTES_UTIL_H_
