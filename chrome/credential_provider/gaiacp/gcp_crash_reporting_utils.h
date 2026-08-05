// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_GCP_CRASH_REPORTING_UTILS_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_GCP_CRASH_REPORTING_UTILS_H_

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace credential_provider {

// Returns the folder in which GCPW can create a crash database
// ("<DataDirectory>\Crashpad"; see `GetDataDirectory()` in gcp_utils.h), or an
// empty path in case of error.
base::FilePath GetFolderForCrashDumps();

// No-op in Chromium builds.
void SetCommonCrashKeys(const base::CommandLine& command_line);

// Returns the system level registry keys for crash dump upload consent.
// Always returns false in Chromium builds.
bool GetGCPWCollectStatsConsent();

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_GCP_CRASH_REPORTING_UTILS_H_
