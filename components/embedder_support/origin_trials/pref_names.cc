// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/origin_trials/pref_names.h"

namespace embedder_support {
namespace prefs {

// The base64-encoded representation of the public key to use to validate origin
// trial token signatures.
const char kOriginTrialPublicKey[] = "origin_trials.public_key";

// A list of origin trial features to disable by policy.
const char kOriginTrialDisabledFeatures[] = "origin_trials.disabled_features";

// A list of origin trial tokens to disable by policy.
const char kOriginTrialDisabledTokens[] = "origin_trials.disabled_tokens";

}  // namespace prefs
}  // namespace embedder_support
