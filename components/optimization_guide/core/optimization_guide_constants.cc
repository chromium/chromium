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

const char kModelValidationErrorHistogramString[] =
    "OptimizationGuide.ModelValidation.OnDevice.DidError";

const char kOptimizationGuideLanguageOverrideHeaderKey[] =
    "x-optimization-guide-language-override";

const base::FilePath::CharType kOptimizationGuideHintStore[] =
    FILE_PATH_LITERAL("optimization_guide_hint_cache_store");

const base::FilePath::CharType
    kOldOptimizationGuidePredictionModelMetadataStore[] =
        FILE_PATH_LITERAL("optimization_guide_model_metadata_store");

const base::FilePath::CharType kOldOptimizationGuidePredictionModelDownloads[] =
    FILE_PATH_LITERAL("optimization_guide_prediction_model_downloads");

const base::FilePath::CharType kOptimizationGuideModelStoreDirPrefix[] =
    FILE_PATH_LITERAL("optimization_guide_model_store");

const base::FilePath::CharType kWeightsFile[] =
    FILE_PATH_LITERAL("weights.bin");

const base::FilePath::CharType kTsDataFile[] = FILE_PATH_LITERAL("ts.bin");

const base::FilePath::CharType kTsSpModelFile[] =
    FILE_PATH_LITERAL("ts_spm.model");

const base::FilePath::CharType kOnDeviceModelExecutionConfigFile[] =
    FILE_PATH_LITERAL("on_device_model_execution_config.pb");

const base::FilePath::CharType kOnDeviceModelAdaptationWeightsFile[] =
    FILE_PATH_LITERAL("adaptation_weights.bin");

}  // namespace optimization_guide
