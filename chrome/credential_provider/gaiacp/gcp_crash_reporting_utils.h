// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_GCP_CRASH_REPORTING_UTILS_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_GCP_CRASH_REPORTING_UTILS_H_

#include "base/strings/string16.h"

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace credential_provider {

class GcpCrashReporterClient;

// Helper function used to prepare the database used by crashpad.
// Since the GCPW dll is run under the SYSTEM account the crash reports will be
// stored in a system level temp folder (see comments for the function
// GetSystemTempFolder in gcp_crash_reporting_utils.cc for how the system temp
// folder is retrieved). The crashpad folder will be under
// "<Temp folder>\GCPW Crashpad".
// After this function call, the caller can request to start an embedded crash
// handler process to listen for crashes on the calling process. When a crash
// occurs on this process, the handler will request that a crash dump be written
// in the crashpad folder and then attempt to upload the crashes to a Crash
// server (if enabled and certain throttling conditions are satisfied).
// Since every DLL_ATTACH event for this dll will try to start crash handling
// there can be multiple entry points to this function via different calls
// to DLL entry points that are exposed. In order to not start one handler per
// entry point that is called, the command line that starts the entry point
// (e.g. the execution of "rundll32.exe gaia1_0.dll,<entry_point>") should also
// provide a "process-type" switch so that these child entry points can re-use
// the same crashpad handler process to report crashes.
void InitializeGcpwCrashReporting(GcpCrashReporterClient* crash_client);

// Returns the system level temp folder in which GCPW can create a crash
// database.
base::FilePath GetFolderForCrashDumps();

// No-op in Chromium builds.
void SetCommonCrashKeys(const base::CommandLine& command_line);

// Returns the system level registry keys for crash dump upload consent.
// Always returns false in Chromium builds.
bool GetGCPWCollectStatsConsent();

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_GCP_CRASH_REPORTING_UTILS_H_
