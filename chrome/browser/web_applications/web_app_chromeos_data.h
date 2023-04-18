// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_CHROMEOS_DATA_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_CHROMEOS_DATA_H_

#include "base/files/file_path.h"
#include "base/values.h"

namespace web_app {

struct WebAppChromeOsData {
  WebAppChromeOsData();
  WebAppChromeOsData(const WebAppChromeOsData&);
  ~WebAppChromeOsData();

  base::Value AsDebugValue() const;

  // By default an app is shown everywhere.
  bool show_in_launcher = true;
  bool show_in_search = true;
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
  // Experimental field to specify the file path of the dedicated app profile
  // within which the app should launch. The value is serialized in Pickle
  // format.
  absl::optional<base::FilePath> app_profile_path;
};

bool operator==(const WebAppChromeOsData& chromeos_data1,
                const WebAppChromeOsData& chromeos_data2);
bool operator!=(const WebAppChromeOsData& chromeos_data1,
                const WebAppChromeOsData& chromeos_data2);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_CHROMEOS_DATA_H_
