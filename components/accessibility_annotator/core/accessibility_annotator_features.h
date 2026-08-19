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
BASE_DECLARE_FEATURE_PARAM(int, kContentAnnotatorMaxCacheAnnotations);
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                           kContentAnnotatorConfirmedStatusLookbackWindow);
BASE_DECLARE_FEATURE_PARAM(bool, kContentAnnotatorEnableMultiTabAnnotations);

BASE_DECLARE_FEATURE(kAccessibilityAnnotator);
BASE_DECLARE_FEATURE(kAccessibilityAnnotatorDatabaseStorage);

}  // namespace accessibility_annotator::features

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_FEATURES_H_
