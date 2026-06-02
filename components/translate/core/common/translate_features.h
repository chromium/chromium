// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_COMMON_TRANSLATE_FEATURES_H_
#define COMPONENTS_TRANSLATE_CORE_COMMON_TRANSLATE_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace translate {

// Controls whether PDF translation is enabled.
BASE_DECLARE_FEATURE(kEnableTranslatePdf);

// Controls whether the simplified Hindi model is used.
BASE_DECLARE_FEATURE(kTranslateSimplifiedHindi);

// Controls whether the target language selection uses the new Search UI.
BASE_DECLARE_FEATURE(kTranslateLanguageSearchUI);

// Controls whether dynamic element experiment features are enabled.
BASE_DECLARE_FEATURE(kTranslateElementExperimentFeatures);
extern const base::FeatureParam<std::string>
    kTranslateElementExperimentFeaturesParam;

// Feature flag for enabling regional endpoints for webpage translation.
BASE_DECLARE_FEATURE(kTranslateElementRegionalization);

// Enables direct usage of oneplatform private translate API.
// If disabled (default), the Contextual Search infrastructure is used to
// perform partial translation.
// If enabled, the OnePlatform Translate API is called directly.
BASE_DECLARE_FEATURE(kPartialTranslateUseOnePlatformApi);

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_COMMON_TRANSLATE_FEATURES_H_
