// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/switches.h"

namespace embedder_support {

// Disable auto-reload of error pages.
const char kDisableAutoReload[] = "disable-auto-reload";

// Disables pop-up blocking.
const char kDisablePopupBlocking[] = "disable-popup-blocking";

// Enable auto-reload of error pages.
const char kEnableAutoReload[] = "enable-auto-reload";

// Enable headless mode.
const char kHeadless[] = "headless";

// Contains a list of feature names for which origin trial experiments should
// be disabled. Names should be separated by "|" characters.
const char kOriginTrialDisabledFeatures[] = "origin-trial-disabled-features";

// Contains a list of token signatures for which origin trial experiments should
// be disabled. Tokens should be separated by "|" characters.
const char kOriginTrialDisabledTokens[] = "origin-trial-disabled-tokens";

// Comma-separated list of keys which will override the default public keys for
// checking origin trial tokens.
const char kOriginTrialPublicKey[] = "origin-trial-public-key";

// Sets the Reporting API delay to under a second to allow much quicker reports.
const char kShortReportingDelay[] = "short-reporting-delay";

// A string used to override the default user agent with a custom one.
const char kUserAgent[] = "user-agent";

}  // namespace embedder_support
