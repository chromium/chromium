// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/chrome_cleaner/public/constants/constants.h"

namespace chrome_cleaner {

// Command line switches.
const char kChromeChannelSwitch[] = "chrome-channel";
const char kChromeExePathSwitch[] = "chrome-exe-path";
const char kChromePromptSwitch[] = "chrome-prompt";
const char kChromeReadHandleSwitch[] = "chrome-read-handle";
const char kChromeWriteHandleSwitch[] = "chrome-write-handle";
const char kChromeSystemInstallSwitch[] = "chrome-system-install";
const char kChromeVersionSwitch[] = "chrome-version";
const char kWithScanningModeLogsSwitch[] = "with-scanning-mode-logs";
const char kEnableCrashReportingSwitch[] = "enable-crash-reporting";
const char kEngineSwitch[] = "engine";
const char kExecutionModeSwitch[] = "execution-mode";
const char kExtendedSafeBrowsingEnabledSwitch[] =
    "extended-safebrowsing-enabled";
const char kRegistrySuffixSwitch[] = "registry-suffix";
const char kSessionIdSwitch[] = "session-id";
const char kSRTPromptFieldTrialGroupNameSwitch[] = "srt-field-trial-group-name";
const char kUmaUserSwitch[] = "uma-user";

// Registry paths and subkeys.
const wchar_t kSoftwareRemovalToolRegistryKey[] =
    L"Software\\Google\\Software Removal Tool";
const wchar_t kCleanerSubKey[] = L"Cleaner";
const wchar_t kScanTimesSubKey[] = L"ScanTimes";

// Registry value names.
const wchar_t kCleanupCompletedValueName[] = L"cleanup-completed";
const wchar_t kEndTimeValueName[] = L"EndTime";
const wchar_t kEngineErrorCodeValueName[] = L"EngineErrorCode";
const wchar_t kExitCodeValueName[] = L"ExitCode";
// Note: the lowercase "s" in "Uws" can't be fixed due to compatibility with
// older versions.
const wchar_t kFoundUwsValueName[] = L"FoundUws";
const wchar_t kLogsUploadResultValueName[] = L"LogsUploadResult";
const wchar_t kMemoryUsedValueName[] = L"MemoryUsed";
const wchar_t kStartTimeValueName[] = L"StartTime";
const wchar_t kUploadResultsValueName[] = L"UploadResults";
const wchar_t kVersionValueName[] = L"Version";

std::ostream& operator<<(std::ostream& stream, ExecutionMode mode) {
  switch (mode) {
    case ExecutionMode::kNone:
      stream << "ExecutionModeNone";
      break;
    case ExecutionMode::kScanning:
      stream << "ExecutionModeScanning";
      break;
    case ExecutionMode::kCleanup:
      stream << "ExecutionModeCleanup";
      break;
    case ExecutionMode::kNumValues:
      stream << "ExecutionModeNumValues";
      break;
  }
  return stream;
}

std::ostream& operator<<(std::ostream& stream, ChromePromptValue value) {
  switch (value) {
    case ChromePromptValue::kUnspecified:
      stream << "ChromePromptUnspecified";
      break;
    case ChromePromptValue::kPrompted:
      stream << "ChromePromptPrompted";
      break;
    case ChromePromptValue::kUserInitiated:
      stream << "ChromePromptUserInitiated";
      break;
    case ChromePromptValue::kLegacyNotPrompted:
      stream << "ChromePromptLegacyNotPrompted";
      break;
    case ChromePromptValue::kLegacyUnknown:
      stream << "ChromePromptLegacyUnknown";
      break;
    case ChromePromptValue::kLegacyShownFromMenu:
      stream << "ChromePromptLegacyShownFromMenu";
      break;
  }
  return stream;
}

}  // namespace chrome_cleaner
