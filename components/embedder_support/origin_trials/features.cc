// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/origin_trials/features.h"

#include "base/feature_list.h"

namespace embedder_support {

// The feature is enabled by default, ensuring user from no-op groups have
// access to the underlying origin trial.
// Users from experiment group will behave the same as default.
// Users from control group will have the feature disabled, excluding them
// from the origin trial.
BASE_FEATURE(kOriginTrialsSampleAPIThirdPartyAlternativeUsage,
             "OriginTrialsSampleAPIThirdPartyAlternativeUsage",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kConversionMeasurementAPIAlternativeUsage,
             "ConversionMeasurementAPIAlternativeUsage",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace embedder_support
