// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_CHROME_PWA_LAUNCHER_CHROME_PWA_LAUNCHER_UTIL_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_CHROME_PWA_LAUNCHER_CHROME_PWA_LAUNCHER_UTIL_H_

#include "base/files/file_path.h"

namespace web_app {

// Returns the path of chrome_pwa_launcher.exe at
// <current directory>/<current version>/chrome_pwa_launcher.exe. Returns an
// empty path if getting the current directory fails.
base::FilePath GetChromePwaLauncherPath();

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_CHROME_PWA_LAUNCHER_CHROME_PWA_LAUNCHER_UTIL_H_
