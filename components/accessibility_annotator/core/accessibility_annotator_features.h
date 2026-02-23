// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_FEATURES_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace accessibility_annotator {

BASE_DECLARE_FEATURE(kContentAnnotator);

extern const base::FeatureParam<int> kContentAnnotatorMaxPendingUrls;
extern const base::FeatureParam<std::string>
    kContentAnnotatorClassifierTitleKeywordRules;
extern const base::FeatureParam<std::string>
    kContentAnnotatorClassifierUrlMatchRules;
extern const base::FeatureParam<std::string>
    kContentAnnotatorClassifierRelevanceValues;
extern const base::FeatureParam<double> kContentAnnotatorSensitivityThreshold;
extern const base::FeatureParam<std::string>
    kContentAnnotatorSupportedLanguages;

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_FEATURES_H_
