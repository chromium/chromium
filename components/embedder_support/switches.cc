// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/switches.h"

namespace embedder_support {

// Disables pop-up blocking.
const char kDisablePopupBlocking[] = "disable-popup-blocking";

// Contains a list of feature names for which origin trial experiments should
// be disabled. Names should be separated by "|" characters.
const char kOriginTrialDisabledFeatures[] = "origin-trial-disabled-features";

// Contains a list of token signatures for which origin trial experiments should
// be disabled. Tokens should be separated by "|" characters.
const char kOriginTrialDisabledTokens[] = "origin-trial-disabled-tokens";

// Comma-separated list of keys which will override the default public keys for
// checking origin trial tokens.
const char kOriginTrialPublicKey[] = "origin-trial-public-key";

// A string used to override the default user agent with a custom one.
const char kUserAgent[] = "user-agent";

}  // namespace embedder_support
