// Copyright 2017 The Chromium Authors
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

const char kOptimizationGuideServiceModelExecutionDefaultURL[] =
    "https://chromemodelexecution-pa.googleapis.com/v1:Execute";

const char kOptimizationGuideServiceModelQualtiyDefaultURL[] =
    "https://chromemodelquality-pa.googleapis.com/v1:LogAiData";

const char kLoadedHintLocalHistogramString[] =
    "OptimizationGuide.LoadedHint.Result";

const base::FilePath::CharType kOptimizationGuideHintStore[] =
    FILE_PATH_LITERAL("optimization_guide_hint_cache_store");

const base::FilePath::CharType
    kOldOptimizationGuidePredictionModelMetadataStore[] =
        FILE_PATH_LITERAL("optimization_guide_model_metadata_store");

const base::FilePath::CharType kOldOptimizationGuidePredictionModelDownloads[] =
    FILE_PATH_LITERAL("optimization_guide_prediction_model_downloads");

const base::FilePath::CharType kOptimizationGuideModelStoreDirPrefix[] =
    FILE_PATH_LITERAL("optimization_guide_model_store");

const base::FilePath::CharType kPageEntitiesMetadataStore[] =
    FILE_PATH_LITERAL("page_content_annotations_page_entities_metadata_store");

}  // namespace optimization_guide
