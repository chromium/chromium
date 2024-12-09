// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_INTERVENTIONS_COMMON_INTERVENTIONS_FEATURES_H_
#define COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_INTERVENTIONS_COMMON_INTERVENTIONS_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace fingerprinting_protection_interventions::features {

// Returns true if the global feature flag state of CanvasInterventions
// blink::RuntimeFeature flag is enabled.
COMPONENT_EXPORT(FINGERPRINTING_PROTECTION_INTERVENTIONS_FEATURES)
bool IsCanvasInterventionsFeatureEnabled();

// TODO(crbug.com/380458351): Add incognito-specific feature enabled accessor.
// TODO(crbug.com/380463018): Add base::FeatureParams for signatures.

}  // namespace fingerprinting_protection_interventions::features

#endif  // COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_INTERVENTIONS_COMMON_INTERVENTIONS_FEATURES_H_
