// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_INTERVENTIONS_COMMON_INTERVENTIONS_FEATURES_H_
#define COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_INTERVENTIONS_COMMON_INTERVENTIONS_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace fingerprinting_protection_interventions::features {

COMPONENT_EXPORT(FINGERPRINTING_PROTECTION_INTERVENTIONS_FEATURES)
BASE_DECLARE_FEATURE(kCanvasNoise);
COMPONENT_EXPORT(FINGERPRINTING_PROTECTION_INTERVENTIONS_FEATURES)
BASE_DECLARE_FEATURE_PARAM(bool, kCanvasNoiseInRegularMode);

// Returns true if canvas interventions should be enabled for the provided
// Incognito state.
COMPONENT_EXPORT(FINGERPRINTING_PROTECTION_INTERVENTIONS_FEATURES)
bool IsCanvasInterventionsEnabledForIncognitoState(bool is_incognito);

// TODO(crbug.com/380463018): Add base::FeatureParams for signatures.

}  // namespace fingerprinting_protection_interventions::features

#endif  // COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_INTERVENTIONS_COMMON_INTERVENTIONS_FEATURES_H_
