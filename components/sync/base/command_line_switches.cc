// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/command_line_switches.h"

#include <string>

#include "base/command_line.h"

namespace syncer {

namespace {

constexpr char kDefaultTrustedVaultServiceURL[] =
    "https://securitydomain-pa.googleapis.com/v1/";

}  // namespace

bool IsSyncAllowedByFlag() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(kDisableSync);
}

GURL ExtractTrustedVaultServiceURLFromCommandLine() {
  std::string string_url =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          syncer::kTrustedVaultServiceURL);
  if (string_url.empty()) {
    // Command line switch is not specified or is not a valid ASCII string.
    return GURL(kDefaultTrustedVaultServiceURL);
  }
  return GURL(string_url);
}

}  // namespace syncer
