// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_CONSTANTS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_CONSTANTS_H_

#include "base/files/file_path.h"

namespace optimization_guide {

// The name of the file that stores the unindexed hints.
extern const base::FilePath::CharType kUnindexedHintsFileName[];

extern const char kRulesetFormatVersionString[];

// The remote Optimization Guide Service production server to fetch hints from.
extern const char kOptimizationGuideServiceGetHintsDefaultURL[];

// The remote Optimization Guide Service production server to fetch models and
// hosts features from.
extern const char kOptimizationGuideServiceGetModelsDefaultURL[];

// The remote Optimization Guide Service production server to execute models.
extern const char kOptimizationGuideServiceModelExecutionDefaultURL[];

// The local histogram used to record that the component hints are stored in
// the cache and are ready for use.
extern const char kLoadedHintLocalHistogramString[];

// The folder where the hint data will be stored on disk.
extern const base::FilePath::CharType kOptimizationGuideHintStore[];

// The folder where the prediction model and associated metadata are
// currently stored on disk. This is per profile.
extern const base::FilePath::CharType
    kOptimizationGuidePredictionModelMetadataStore[];

// The folder where the prediction model downloads are stored. This is per
// profile.
extern const base::FilePath::CharType
    kOptimizationGuidePredictionModelDownloads[];

// The folder where the page entities metadata store will be stored on disk.
extern const base::FilePath::CharType kPageEntitiesMetadataStore[];

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_CONSTANTS_H_
