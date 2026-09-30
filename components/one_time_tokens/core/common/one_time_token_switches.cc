// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/common/one_time_token_switches.h"

namespace one_time_tokens::switches {

const char kOneTimeTokenServiceBaseUrl[] = "one-time-token-service-base-url";

const char kDefaultOneTimeTokenServiceBaseUrl[] =
    "https://onetimetoken.pa.googleapis.com";

// Returns the specified mock OTP string value when retrieving OTP tokens.
// Note: In Actor OTP flows (AttemptOtpFillingTool), if
// actor::switches::kAttemptOtpFillingToolMockValueSkipsChecks is also present,
// it takes precedence and additionally bypasses consent and confirmation
// checks.
const char kMockOtpValue[] = "mock-otp-value";

}  // namespace one_time_tokens::switches
