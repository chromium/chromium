// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CHROME_CLEANER_PUBLIC_CONSTANTS_CONSTANTS_H_
#define COMPONENTS_CHROME_CLEANER_PUBLIC_CONSTANTS_CONSTANTS_H_

#include <ostream>

#include "components/chrome_cleaner/public/constants/result_codes.h"

// Constants shared by the Chromium and the Chrome Cleanaup tool repos.

namespace chrome_cleaner {

// Switches sent from Chrome to either the Software Reporter or the Chrome
// Cleanup tool.

// The current Chrome channel. The value of this flag is an integer with values
// according to version_info::Channel enum.
extern const char kChromeChannelSwitch[];

// The path to Chrome's executable.
extern const char kChromeExePathSwitch[];

// Indicates that a cleaner run was started by Chrome.
extern const char kChromePromptSwitch[];

// Handle to the read end of a pipe for communicating with Chrome.
extern const char kChromeReadHandleSwitch[];

// Handle to the write end of a pipe for communicating with Chrome.
extern const char kChromeWriteHandleSwitch[];

// Indicates that the current Chrome installation was a system-level
// installation.
extern const char kChromeSystemInstallSwitch[];

// The Chrome version string.
extern const char kChromeVersionSwitch[];

// Identify that the cleaner process in scanning mode is allowed to collect
// logs. This should only be set if |kExecutionModeSwitch| is
// ExecutionMode::kScanning.
extern const char kWithScanningModeLogsSwitch[];

// Indicates that crash reporting is enabled for the current user.
extern const char kEnableCrashReportingSwitch[];

// Specify the engine to use.
extern const char kEngineSwitch[];

// Indicates the execution mode for the Chrome Cleanup Tool. Possible values
// defined in enum ExecutionMode.
extern const char kExecutionModeSwitch[];

// Indicates that the current user opted into Safe Browsing Extended Reporting.
// This should not be used by non-legacy-mode Chrome Cleanup Tool.
extern const char kExtendedSafeBrowsingEnabledSwitch[];

// Specifies the suffix to the registry path where metrics data will be saved.
extern const char kRegistrySuffixSwitch[];

// Identifier used to group all reports generated during the same run of the
// software reporter (which may include multiple invocations of the reporter
/// binary, each generating a report). An ASCII, base-64 encoded random string.
extern const char kSessionIdSwitch[];

// Indicates the group name for the SRTPrompt field trial.
extern const char kSRTPromptFieldTrialGroupNameSwitch[];

// Indicates that metrics reporting is enabled for the current user.
extern const char kUmaUserSwitch[];

// Registry paths where the reporter and the cleaner will write metrics data
// to be reported by Chrome.

// TODO(b/647763) Change the registry key to properly handle cases when the
// user runs Google Chrome stable alongside Google Chrome SxS.
extern const wchar_t kSoftwareRemovalToolRegistryKey[];

// The suffix for the registry key where cleaner metrics are written to.
extern const wchar_t kCleanerSubKey[];
// The suffix for registry key paths where scan times will be written to.
extern const wchar_t kScanTimesSubKey[];

// Registry value names that indicate if a cleanup has completed.
extern const wchar_t kCleanupCompletedValueName[];

// Registry value names where metrics are written to.
extern const wchar_t kEndTimeValueName[];
extern const wchar_t kEngineErrorCodeValueName[];
extern const wchar_t kExitCodeValueName[];
extern const wchar_t kFoundUwsValueName[];
extern const wchar_t kLogsUploadResultValueName[];
extern const wchar_t kMemoryUsedValueName[];
extern const wchar_t kStartTimeValueName[];
extern const wchar_t kUploadResultsValueName[];
extern const wchar_t kVersionValueName[];

// Exit codes from the Software Reporter process identified by Chrome.
constexpr int kSwReporterCleanupNeeded = RESULT_CODE_SUCCESS;
constexpr int kSwReporterNothingFound = RESULT_CODE_NO_PUPS_FOUND;
constexpr int kSwReporterPostRebootCleanupNeeded =
    DEPRECATED_RESULT_CODE_ABOUT_TO_REBOOT;
constexpr int kSwReporterNonRemovableOnly =
    RESULT_CODE_EXAMINED_FOR_REMOVAL_ONLY;
constexpr int kSwReporterDelayedPostRebootCleanupNeeded =
    RESULT_CODE_PENDING_REBOOT;
constexpr int kSwReporterSuspiciousOnly = RESULT_CODE_REPORT_ONLY_PUPS_FOUND;
constexpr int kSwReporterTimeoutWithoutUwS =
    RESULT_CODE_WATCHDOG_TIMEOUT_WITHOUT_REMOVABLE_UWS;
constexpr int kSwReporterTimeoutWithUwS =
    RESULT_CODE_WATCHDOG_TIMEOUT_WITH_REMOVABLE_UWS;

// Values to be passed to the kChromePromptSwitch of the Chrome Cleanup Tool to
// indicate how the user interacted with the accept button.
enum class ChromePromptValue {
  // Value not set.
  kUnspecified = 0,
  // The user accepted the prompt when the prompt was first shown.
  kPrompted = 3,
  // The user started the cleanup from the Settings page.
  kUserInitiated = 5,

  // Legacy values that shouldn't be used in Chromium code.
  kLegacyNotPrompted = 1,
  kLegacyUnknown = 2,
  kLegacyShownFromMenu = 4,
};

// Values to be passed to the kExecutionModeSwitch for the Chrome Cleanup Tool
// to indicate the mode in which it should be executed.
enum class ExecutionMode {
  // No mode specified, which means the cleaner is running in legacy mode and
  // will show its own UI and handle logs uploading permissions.
  kNone = 0,
  // The cleaner will run in scanning mode. No UI will be shown to the user
  // (UI handled by Chrome) and logs will not be uploaded.
  kScanning = 1,
  // The cleaner will run in cleaning mode. No UI will be shown to the user
  // (UI handled by Chrome) and logs will be uploaded if the user did not opt
  // out of logs collection when it was offered by the Chrome UI.
  // Chrome should not try to launch the Chrome Cleanup Tool with |kCleanup|.
  // It should instead communicate through IPC with the cleaner launched with
  // |kScanning| to ask it to start cleanup.
  kCleanup = 2,

  // Auxiliary enumerator for range checking.
  kNumValues,
};

// Pretty printers for gtest and CHECK. Declared here to avoid ODR violations.
// See explanation at
// https://groups.google.com/a/chromium.org/d/msg/chromium-dev/i_wOTsE5Z6g/jhtqTY6fCwAJ.

std::ostream& operator<<(std::ostream& stream, ChromePromptValue mode);

std::ostream& operator<<(std::ostream& stream, ExecutionMode mode);

}  // namespace chrome_cleaner

#endif  // COMPONENTS_CHROME_CLEANER_PUBLIC_CONSTANTS_CONSTANTS_H_
