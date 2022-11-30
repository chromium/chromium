// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_CHROME_PWA_LAUNCHER_LAUNCHER_LOG_REPORTER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_CHROME_PWA_LAUNCHER_LAUNCHER_LOG_REPORTER_H_

namespace web_app {

// Records the result of the last launch attempt by a PWA launcher (the
// PWALauncherResult registry value logged by LauncherLog) to UMA, then deletes
// it from the registry.
void RecordPwaLauncherResult();

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_CHROME_PWA_LAUNCHER_LAUNCHER_LOG_REPORTER_H_
