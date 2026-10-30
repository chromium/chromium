// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_FEATURES_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"

namespace variations {

// Skips invalidating cached variation headers when a finalized field trial has
// no associated `VariationID`, and skips notifying observers when regenerated
// headers are unchanged.
COMPONENT_EXPORT(VARIATIONS)
BASE_DECLARE_FEATURE(kOptimizeVariationsIpc);

namespace internal {

// Add Variations Features here...

}  // namespace internal
}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_FEATURES_H_
