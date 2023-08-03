// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/invalidation_switches.h"

#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace invalidation {
namespace switches {

namespace {

// Default TTL (if the SyncInstanceIDTokenTTL/PolicyInstanceIDTokenTTL feature
// is enabled) is 2 weeks. Exposed for testing.
const int kDefaultInstanceIDTokenTTLSeconds = 14 * 24 * 60 * 60;

}  // namespace

BASE_FEATURE(kPolicyInstanceIDTokenTTL,
             "PolicyInstanceIDTokenTTL",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kPolicyInstanceIDTokenTTLSeconds{
    &kPolicyInstanceIDTokenTTL, "time_to_live_seconds",
    kDefaultInstanceIDTokenTTLSeconds};

}  // namespace switches
}  // namespace invalidation
