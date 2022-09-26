// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_ORIGIN_TRIALS_FEATURES_H_
#define COMPONENTS_EMBEDDER_SUPPORT_ORIGIN_TRIALS_FEATURES_H_

#include "base/feature_list.h"

namespace embedder_support {

// Sample field trial feature for testing alternative usage restriction in
// origin trial third party tokens.
BASE_DECLARE_FEATURE(kOriginTrialsSampleAPIThirdPartyAlternativeUsage);

// Field trial feature for controlling usage restriction of the
// Conversion Measurement API's origin trial tokens. When disabled, the API
// cannot be enabled by tokens using the user subset usage restriction.
BASE_DECLARE_FEATURE(kConversionMeasurementAPIAlternativeUsage);

}  // namespace embedder_support

#endif  // COMPONENTS_EMBEDDER_SUPPORT_ORIGIN_TRIALS_FEATURES_H_
