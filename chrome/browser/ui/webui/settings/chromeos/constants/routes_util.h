// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CONSTANTS_ROUTES_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CONSTANTS_ROUTES_UTIL_H_

#include <string>

namespace chromeos {
namespace settings {

// TODO(khorimoto): Remove this path, since it's no longer used.
extern const char kOsSignOutSubPage[];

// Returns true if the sub-page is one of the above.
bool IsOSSettingsSubPage(const std::string& sub_page);

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CONSTANTS_ROUTES_UTIL_H_
