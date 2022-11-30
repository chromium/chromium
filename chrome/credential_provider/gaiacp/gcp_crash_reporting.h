// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_GCP_CRASH_REPORTING_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_GCP_CRASH_REPORTING_H_

namespace base {
class CommandLine;
}  // namespace base

namespace credential_provider {

// Sets up the crash reporting system for the actual credential provider.
// This function also sets all crash keys that are available during process
// startup. Also sets crash keys for the switches given in |command_line| and a
// crash key recording the version of the product which was present before the
// installer was run.
void ConfigureGcpCrashReporting(const base::CommandLine& command_line);

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_GCP_CRASH_REPORTING_H_
