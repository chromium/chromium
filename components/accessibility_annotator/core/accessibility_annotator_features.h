// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_FEATURES_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace accessibility_annotator {

BASE_DECLARE_FEATURE(kContentAnnotator);
BASE_DECLARE_FEATURE(kAccessibilityAnnotator);

BASE_DECLARE_FEATURE_PARAM(int, kContentAnnotatorMaxPendingUrls);
BASE_DECLARE_FEATURE_PARAM(std::string,
                           kContentAnnotatorClassifierTitleKeywordRules);
BASE_DECLARE_FEATURE_PARAM(std::string,
                           kContentAnnotatorClassifierUrlMatchRules);
BASE_DECLARE_FEATURE_PARAM(std::string,
                           kContentAnnotatorClassifierRelevanceValues);
BASE_DECLARE_FEATURE_PARAM(double, kContentAnnotatorSensitivityThreshold);
BASE_DECLARE_FEATURE_PARAM(std::string, kContentAnnotatorSupportedLanguages);
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kContentAnnotatorAnnotationTimeout);
BASE_DECLARE_FEATURE_PARAM(bool, kContentAnnotatorEnableFullAnnotation);
BASE_DECLARE_FEATURE_PARAM(bool, kContentAnnotatorLanguageCheckEnabled);
BASE_DECLARE_FEATURE_PARAM(int, kContentAnnotatorMaxCacheAnnotations);
BASE_DECLARE_FEATURE_PARAM(std::string,
                           kContentAnnotatorClassifierSemanticMatchRules);
BASE_DECLARE_FEATURE_PARAM(double, kContentAnnotatorSemanticMatchThreshold);

BASE_DECLARE_FEATURE(kAccessibilityAnnotationReducerOnePResolver);

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_FEATURES_H_
