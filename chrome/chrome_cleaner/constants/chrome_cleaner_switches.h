// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_CONSTANTS_CHROME_CLEANER_SWITCHES_H_
#define CHROME_CHROME_CLEANER_CONSTANTS_CHROME_CLEANER_SWITCHES_H_

#include "chrome/chrome_cleaner/buildflags.h"

namespace chrome_cleaner {

// Command line switches.
extern const char kCleaningTimeoutMinutesSwitch[];
extern const char kCleanupIdSwitch[];
extern const char kCrashHandlerSwitch[];
extern const char kCrashSwitch[];
extern const char kDumpRawLogsSwitch[];
extern const char kElevatedSwitch[];
extern const char kFileSizeLimitSwitch[];
extern const char kForceLogsUploadFailureSwitch[];
extern const char kForceRecoveryComponentSwitch[];
extern const char kForceSelfDeleteSwitch[];
extern const char kInitDoneNotifierSwitch[];
extern const char kIntegrationTestTimeoutMinutesSwitch[];
extern const char kLoadEmptyDLLSwitch[];
extern const char kLogInterfaceCallsToSwitch[];
extern const char kLogUploadRetryIntervalSwitch[];
extern const char kNoCrashUploadSwitch[];
extern const char kNoRecoveryComponentSwitch[];
extern const char kNoReportUploadSwitch[];
extern const char kNoSelfDeleteSwitch[];
extern const char kPostRebootSwitch[];
extern const char kPostRebootSwitchesInOtherRegistryKeySwitch[];
extern const char kPostRebootTriggerSwitch[];
extern const char kQuarantineDirSwitch[];
extern const char kRemoveScanOnlyUwS[];
extern const char kSandboxMojoPipeTokenSwitch[];
extern const char kSandboxedProcessIdSwitch[];
extern const char kScanLocationsSwitch[];
extern const char kScanningTimeoutMinutesSwitch[];
extern const char kTestLoggingPathSwitch[];
extern const char kTestLoggingURLSwitch[];
extern const char kTestingSwitch[];
extern const char kUploadLogFileSwitch[];
extern const char kUseCrashHandlerInTestsSwitch[];
extern const char kUseCrashHandlerWithIdSwitch[];
extern const char kUseTempRegistryPathSwitch[];
extern const char kUserResponseTimeoutMinutesSwitch[];
extern const char kWithCleanupModeLogsSwitch[];

// Unoffical build only switches.
#if !BUILDFLAG(IS_OFFICIAL_CHROME_CLEANER_BUILD)
extern const char kAllowUnsecureDLLsSwitch[];
extern const char kRunWithoutSandboxForTestingSwitch[];
#endif

// Deprecated switches that were set by older Chrome versions.
// These must still be handled until we drop support for those versions.

// The Mojo pipe token for IPC communication between the Software Reporter and
// Chrome. Dropped in M80.
extern const char kChromeMojoPipeTokenSwitch[];

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_CONSTANTS_CHROME_CLEANER_SWITCHES_H_
