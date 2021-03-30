// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_constants.h"

namespace optimization_guide {

const base::FilePath::CharType kUnindexedHintsFileName[] =
    FILE_PATH_LITERAL("optimization-hints.pb");

const char kRulesetFormatVersionString[] = "1.0.0";

const char kOptimizationGuideServiceGetHintsDefaultURL[] =
    "https://optimizationguide-pa.googleapis.com/v1:GetHints";

const char kOptimizationGuideServiceGetModelsDefaultURL[] =
    "https://optimizationguide-pa.googleapis.com/v1:GetModels";

const char kLoadedHintLocalHistogramString[] =
    "OptimizationGuide.LoadedHint.Result";

const char kOptimizationGuideHintStore[] = "previews_hint_cache_store";

const char kOptimizationGuidePredictionModelAndFeaturesStore[] =
    "optimization_guide_model_and_features_store";

}  // namespace optimization_guide
