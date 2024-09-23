// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_CONSTANTS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_CONSTANTS_H_

#include "base/component_export.h"
#include "base/files/file_path.h"

namespace optimization_guide {

// The name of the file that stores the unindexed hints.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const base::FilePath::CharType kUnindexedHintsFileName[];

COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kRulesetFormatVersionString[];

// The remote Optimization Guide Service production server to fetch hints from.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOptimizationGuideServiceGetHintsDefaultURL[];

// The remote Optimization Guide Service production server to fetch models and
// hosts features from.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOptimizationGuideServiceGetModelsDefaultURL[];

// The remote Optimization Guide Service production server to execute models.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOptimizationGuideServiceModelExecutionDefaultURL[];

// The remote Optimization Guide Service model quality server to log data.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOptimizationGuideServiceModelQualtiyDefaultURL[];

// The local histogram used to record that the component hints are stored in
// the cache and are ready for use.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kLoadedHintLocalHistogramString[];

// The local histogram used to record that the on-device model validation
// completed with an error.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kModelValidationErrorHistogramString[];

// The name of the language override request header.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOptimizationGuideLanguageOverrideHeaderKey[];

// The folder where the hint data will be stored on disk.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const base::FilePath::CharType kOptimizationGuideHintStore[];

// The folder where the old prediction model and associated metadata are
// currently stored on disk. This is per profile.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const base::FilePath::CharType
    kOldOptimizationGuidePredictionModelMetadataStore[];

// The folder where the old prediction model downloads are stored. This is per
// profile.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const base::FilePath::CharType
    kOldOptimizationGuidePredictionModelDownloads[];

// The prefix for the folder where models are stored by the new install-wide
// model store.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const base::FilePath::CharType kOptimizationGuideModelStoreDirPrefix[];

// Files expected to be in the on device model bundle.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const base::FilePath::CharType kWeightsFile[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const base::FilePath::CharType kOnDeviceModelExecutionConfigFile[];

// Files expected to be in the text safety model bundle.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const base::FilePath::CharType kTsDataFile[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const base::FilePath::CharType kTsSpModelFile[];

// Files expected to be in the on device model adaptation bundle.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const base::FilePath::CharType kOnDeviceModelAdaptationWeightsFile[];

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_CONSTANTS_H_
