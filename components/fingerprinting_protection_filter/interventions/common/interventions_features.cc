// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/interventions/common/interventions_features.h"

#include "base/feature_list.h"

namespace fingerprinting_protection_interventions::features {

// Whether the canvas interventions should be enabled that add noise to the
// readback values.
BASE_FEATURE(kCanvasNoise, base::FeatureState::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(bool,
                   kCanvasNoiseInRegularMode,
                   &kCanvasNoise,
                   "enable_in_regular_mode",
                   false);

// [Experimental] Whether readback of canvases should be blocked.
BASE_FEATURE(kBlockCanvasReadback,
             base::FeatureState::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(bool,
                   kBlockCanvasReadbackInRegularMode,
                   &kBlockCanvasReadback,
                   "enable_in_regular_mode",
                   false);

bool IsCanvasInterventionsEnabledForIncognitoState(bool is_incognito) {
  if (is_incognito) {
    return base::FeatureList::IsEnabled(kCanvasNoise);
  }
  return base::FeatureList::IsEnabled(kCanvasNoise) &&
         kCanvasNoiseInRegularMode.Get();
}

bool ShouldBlockCanvasReadbackForIncognitoState(bool is_incognito) {
  if (is_incognito) {
    return base::FeatureList::IsEnabled(kBlockCanvasReadback);
  }
  return base::FeatureList::IsEnabled(kBlockCanvasReadback) &&
         kBlockCanvasReadbackInRegularMode.Get();
}

}  // namespace fingerprinting_protection_interventions::features
