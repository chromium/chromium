// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/gcpw_strings.h"

namespace credential_provider {
// Time parameters to control validity of the offline session.
const char kKeyLastTokenValid[] = "last_token_valid_millis";
const char kKeyValidityPeriodInDays[] = "validity_period_in_days";
// DEPRECATED
const char kKeyLastSuccessfulOnlineLoginMillis[] =
    "last_successful_online_login_millis";
const wchar_t kKeyAcceptTos[] = L"accept_tos";
const wchar_t kKeyEnableGemFeatures[] = L"enable_gem_features";
const char kGaiaSetupPath[] = "embedded/setup/windows";

// URL for the GEM service handling GCPW requests.
const wchar_t kDefaultGcpwServiceUrl[] = L"https://gcpw-pa.googleapis.com";
}  // namespace credential_provider
