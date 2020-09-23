// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_CONSTANTS_H_
#define CHROME_UPDATER_CONSTANTS_H_

#include "build/build_config.h"
#include "components/update_client/update_client_errors.h"

namespace updater {

// The updater specific app ID.
extern const char kUpdaterAppId[];

// "0.0.0.0". Historically, a null version has been used to indicate a
// new install.
extern const char kNullVersion[];

// Command line switches.
//

// This switch starts the COM server. This switch is invoked by the COM runtime
// when CoCreate is called on one of several CLSIDs that the server supports.
// We expect to use the COM server for the following scenarios:
// * The Server for the UI when installing User applications, and as a fallback
//   if the Service fails when installing Machine applications.
// * The On-Demand COM Server when running for User, and as a fallback if the
//   Service fails.
// * COM Server for launching processes at Medium Integrity, i.e., de-elevating.
// * The On-Demand COM Server when the client requests read-only interfaces
//   (say, Policy queries) at Medium Integrity.
// * A COM broker when running modes such as On-Demand for Machine. A COM broker
//   is something that acts as an intermediary, allowing creating one of several
//   possible COM objects that can service a particular COM request, ranging
//   from Privileged Services that can offer full functionality for a particular
//   set of interfaces, to Medium Integrity processes that can offer limited
//   (say, read-only) functionality for that same set of interfaces.
extern const char kServerSwitch[];

// This switch specifies the XPC service the server registers to listen to.
extern const char kServerServiceSwitch[];

// Valid values for the kServerServiceSwitch.
extern const char kServerControlServiceSwitchValue[];
extern const char kServerUpdateServiceSwitchValue[];

// This switch starts the COM service. This switch is invoked by the Service
// Manager when CoCreate is called on one of several CLSIDs that the server
// supports.
// We expect to use the COM service for the following scenarios:
// * The Server for the UI when installing Machine applications.
// * The On-Demand COM Server for Machine applications.
// * COM Server for launching processes at System Integrity, i.e., an Elevator.
extern const char kComServiceSwitch[];

// Crash the program for testing purposes.
extern const char kCrashMeSwitch[];

// Runs as the Crashpad handler.
extern const char kCrashHandlerSwitch[];

// Updates the updater.
extern const char kUpdateSwitch[];

// Installs the updater.
extern const char kInstallSwitch[];

#if defined(OS_WIN)
// A debug switch to indicate that --install is running from the `out` directory
// of the build. When this switch is present, the setup picks up the run time
// dependencies of the updater from the `out` directory instead of using the
// metainstaller uncompressed archive.
extern const char kInstallFromOutDir[];
#endif  // OS_WIN

// Uninstalls the updater.
extern const char kUninstallSwitch[];

// Kicks off the update service. This switch is typically used for by a
// scheduled to invoke the updater periodically.
extern const char kWakeSwitch[];

// The updater needs to operate in the system context.
extern const char kSystemSwitch[];

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

// Specifies the application that the Updater needs to install.
extern const char kAppIdSwitch[];

// Specifies the version of the application that the updater needs to register.
extern const char kAppVersionSwitch[];

// URLs.
//
// Omaha server end point.
extern const char kUpdaterJSONDefaultUrl[];

// The URL where crash reports are uploaded.
extern const char kCrashUploadURL[];
extern const char kCrashStagingUploadURL[];

// DM server end point.
extern const char kDeviceManagementServerURL[];

// File system paths.
//
// The directory name where CRX apps get installed. This is provided for demo
// purposes, since products installed by this updater will be installed in
// their specific locations.
extern const char kAppsDir[];

// The name of the uninstall script which is invoked by the --uninstall switch.
extern const char kUninstallScript[];

// Developer override keys.
extern const char kDevOverrideKeyUrl[];
extern const char kDevOverrideKeyUseCUP[];

#if defined(OS_WIN)
// Timing constants.
//
// How long to wait for an application installer (such as
// chrome_installer.exe) to complete.
constexpr int kWaitForAppInstallerSec = 60;

// How often the installer progress from registry is sampled. This value may
// be changed to provide a smoother progress experience (crbug.com/1067475).
constexpr int kWaitForInstallerProgressSec = 1;
#endif  // OS_WIN

// Install Errors.
//
// Specific install errors for the updater are reported in such a way that
// their range does not conflict with the range of generic errors defined by
// the |update_client| module.
constexpr int kCustomInstallErrorBase =
    static_cast<int>(update_client::InstallError::CUSTOM_ERROR_BASE);

// The install directory for the application could not be created.
constexpr int kErrorCreateAppInstallDirectory = kCustomInstallErrorBase;

// The install params are missing. This usually means that the update
// response does not include the name of the installer and its command line
// arguments.
constexpr int kErrorMissingInstallParams = kCustomInstallErrorBase + 1;

// The file specified by the manifest |run| attribute could not be found
// inside the CRX.
constexpr int kErrorMissingRunableFile = kCustomInstallErrorBase + 2;

// Running the application installer failed.
constexpr int kErrorApplicationInstallerFailed = kCustomInstallErrorBase + 3;

// Server Errors.
//
// The server process may exit with any of these exit codes.
constexpr int kErrorOk = 0;

// The server could not acquire the lock needed to run.
constexpr int kErrorFailedToLockPrefsMutex = 1;

// The server candidate failed to promote itself to active.
constexpr int kErrorFailedToSwap = 2;

// The server has finished qualification and will not do further operations.
constexpr int kErrorQualificationExit = 3;

// Policy Management constants.
extern const char kProxyModeDirect[];
extern const char kProxyModeAutoDetect[];
extern const char kProxyModePacScript[];
extern const char kProxyModeFixedServers[];
extern const char kProxyModeSystem[];

extern const char kDownloadPreferenceCacheable[];

constexpr int kPolicyDisabled = 0;
constexpr int kPolicyEnabled = 1;
constexpr int kPolicyEnabledMachineOnly = 4;
constexpr int kPolicyManualUpdatesOnly = 2;
constexpr int kPolicyAutomaticUpdatesOnly = 3;

constexpr bool kInstallPolicyDefault = kPolicyEnabled;
constexpr bool kUpdatePolicyDefault = kPolicyEnabled;

}  // namespace updater

#endif  // CHROME_UPDATER_CONSTANTS_H_
