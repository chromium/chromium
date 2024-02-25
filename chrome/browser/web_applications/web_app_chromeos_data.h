// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_CHROMEOS_DATA_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_CHROMEOS_DATA_H_

#include "base/values.h"

namespace web_app {

struct WebAppChromeOsData {
  base::Value AsDebugValue() const;

  // By default an app is shown everywhere.
  bool show_in_launcher = true;
  bool show_in_search_and_shelf = true;
  bool show_in_management = true;
  // By default the app is not disabled. Disabling the app means having a
  // blocked logo on top of the app icon, and the user can't launch the app.
  bool is_disabled = false;
  // True if the app was installed by the device OEM and should be shown
  // in an OEM folder in the app launcher. This could also be stored as a Source
  // on the WebApp, which would require refactoring PreinstalledWebAppManager to
  // manage multiple Sources for a single app.
  bool oem_installed = false;
  bool handles_file_open_intents = show_in_launcher;
};

bool operator==(const WebAppChromeOsData& chromeos_data1,
                const WebAppChromeOsData& chromeos_data2);
bool operator!=(const WebAppChromeOsData& chromeos_data1,
                const WebAppChromeOsData& chromeos_data2);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_CHROMEOS_DATA_H_
