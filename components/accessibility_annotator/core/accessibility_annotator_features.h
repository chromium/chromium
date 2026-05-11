// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_FEATURES_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace accessibility_annotator::features {

BASE_DECLARE_FEATURE(kContentAnnotator);
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
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                           kContentAnnotatorConfirmedStatusLookbackWindow);
BASE_DECLARE_FEATURE_PARAM(std::string,
                           kContentAnnotatorClassifierSemanticMatchRules);
BASE_DECLARE_FEATURE_PARAM(double, kContentAnnotatorSemanticMatchThreshold);
BASE_DECLARE_FEATURE_PARAM(bool, kContentAnnotatorEnableMultiTabAnnotations);

BASE_DECLARE_FEATURE(kAccessibilityAnnotator);
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kAccessibilityAnnotationTTL);

BASE_DECLARE_FEATURE(kAccessibilityAnnotatorFirstRun);

BASE_DECLARE_FEATURE(kAccessibilityAnnotatorFirstRunInfoPhase2);

BASE_DECLARE_FEATURE(kAccessibilityAnnotatorFirstRunSetup);

BASE_DECLARE_FEATURE(kAccessibilityAnnotatorGetEntities);

BASE_DECLARE_FEATURE(kAccessibilityAnnotatorLiveTabContext);
BASE_DECLARE_FEATURE_PARAM(
    int,
    kAccessibilityAnnotatorLiveTabContextMaxSearchResults);
BASE_DECLARE_FEATURE_PARAM(
    int,
    kAccessibilityAnnotatorLiveTabContextPassagesPerPage);
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                           kAccessibilityAnnotatorLiveTabContextRequestTimeout);

BASE_DECLARE_FEATURE(kAccessibilityAnnotationReducerOnePResolver);
BASE_DECLARE_FEATURE_PARAM(std::string, kAccessibilityAnnotatorOnePServiceUrl);

BASE_DECLARE_FEATURE(kAccessibilityAnnotatorDatabaseStorage);

// Returns true if Phase 1, Info Phase 2, or the setup experience of the first
// run experience is enabled. Since Info Phase 2 and the setup experience are
// extensions/continuations of the first run experience, checking all features
// ensures the whole first run feature set is appropriately covered.
bool IsAccessibilityAnnotatorFirstRunEnabled();

// Returns true if Info Phase 2 of the first run experience is enabled.
bool IsAccessibilityAnnotatorFirstRunInfoPhase2Enabled();

// Returns true if the setup experience of the first run experience is enabled.
bool IsAccessibilityAnnotatorFirstRunSetupEnabled();

}  // namespace accessibility_annotator::features

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_FEATURES_H_
