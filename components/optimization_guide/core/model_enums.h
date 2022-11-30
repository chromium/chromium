// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_ENUMS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_ENUMS_H_

namespace optimization_guide {

// The types of decisions that can be made for an optimization target.
//
// Keep in sync with OptimizationGuideOptimizationTargetDecision in enums.xml.
enum class OptimizationTargetDecision {
  kUnknown = 0,
  // The page load does not match the optimization target.
  kPageLoadDoesNotMatch = 1,
  // The page load matches the optimization target.
  kPageLoadMatches = 2,
  // The model needed to make the target decision was not available on the
  // client.
  kModelNotAvailableOnClient = 3,
  // The page load is part of a model prediction holdback where all decisions
  // will return |OptimizationGuideDecision::kFalse| in an attempt to not taint
  // the data for understanding the production recall of the model.
  kModelPredictionHoldback = 4,
  // The OptimizationGuideDecider was not initialized yet.
  kDeciderNotInitialized = 5,

  // Add new values above this line.
  kMaxValue = kDeciderNotInitialized,
};

// The statuses for a prediction model in the prediction manager when requested
// to be evaluated.
//
// Keep in sync with OptimizationGuidePredictionManagerModelStatus in enums.xml.
enum class PredictionManagerModelStatus {
  kUnknown = 0,
  // The model is loaded and available for use.
  kModelAvailable = 1,
  // The store is initialized but does not contain a model for the optimization
  // target.
  kStoreAvailableNoModelForTarget = 2,
  // The store is initialized and contains a model for the optimization target
  // but it is not loaded in memory.
  kStoreAvailableModelNotLoaded = 3,
  // The store is not initialized and it is unknown if it contains a model for
  // the optimization target.
  kStoreUnavailableModelUnknown = 4,

  // Add new values above this line.
  kMaxValue = kStoreUnavailableModelUnknown,
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_ENUMS_H_
