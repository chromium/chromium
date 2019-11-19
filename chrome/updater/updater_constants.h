// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UPDATER_CONSTANTS_H_
#define CHROME_UPDATER_UPDATER_CONSTANTS_H_

namespace updater {

// The updater specific app ID.
extern const char kUpdaterAppId[];

// Chrome's app ID.
extern const char kChromeAppId[];

// Command line switches.
//
// Crash the program for testing purposes.
extern const char kCrashMeSwitch[];

// Runs as the Crashpad handler.
extern const char kCrashHandlerSwitch[];

// Installs the updater.
extern const char kInstallSwitch[];

// Uninstalls the updater.
extern const char kUninstallSwitch[];

// Updates all apps registered with the updater.
extern const char kUpdateAppsSwitch[];

// Runs in test mode. Currently, it exits right away.
extern const char kTestSwitch[];

// Disables throttling for the crash reported until the following bug is fixed:
// https://bugs.chromium.org/p/crashpad/issues/detail?id=23
extern const char kNoRateLimitSwitch[];

// The handle of an event to signal when the initialization of the main process
// is complete.
extern const char kInitDoneNotifierSwitch[];

// Enables logging.
extern const char kEnableLoggingSwitch[];

// Specifies the logging module filter.
extern const char kLoggingModuleSwitch[];

// URLs.
//
// Omaha server end point.
extern const char kUpdaterJSONDefaultUrl[];

// The URL where crash reports are uploaded.
extern const char kCrashUploadURL[];
extern const char kCrashStagingUploadURL[];

// File system paths.
//
// The directory name where CRX apps get installed. This is provided for demo
// purposes, since products installed by this updater will be installed in
// their specific locations.
extern const char kAppsDir[];

// The name of the uninstall script which is invoked by the --uninstall switch.
extern const char kUninstallScript[];

// Errors.
//
// The install directory for the application could not be created.
const int kCustomInstallErrorCreateAppInstallDirectory = 0;

}  // namespace updater

#endif  // CHROME_UPDATER_UPDATER_CONSTANTS_H_
