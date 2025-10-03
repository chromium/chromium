// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_FEATURES_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"

namespace variations::internal {

// A feature to allow removing the finch seed data from memory when it is no
// longer needed.
COMPONENT_EXPORT(VARIATIONS_FEATURES)
BASE_DECLARE_FEATURE(kPurgeVariationsSeedFromMemory);

}  // namespace variations::internal

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_FEATURES_H_
