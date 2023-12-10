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

// The various reasons a prediction model in the model store is being removed.
// Keep in sync with OptimizationGuidePredictionModelStoreModelRemovalReason in
// enums.xml.
enum class PredictionModelStoreModelRemovalReason {
  kUnknown = 0,
  // Model was expired.
  kModelExpired = 1,
  // Model was found to be expired when the model is loaded.
  kModelExpiredOnLoadModel = 2,
  // Model dirs were invalid.
  kInvalidModelDir = 3,
  // Failed when loading the model files from the store.
  kModelLoadFailed = 4,
  // Model file path verification failed on a model update.
  kModelUpdateFilePathVerifyFailed = 5,
  // Model version is invalid.
  kModelVersionInvalid = 6,
  // Remote optimization guide service returned no model in the
  // GetModelsResponse.
  kNoModelInGetModelsResponse = 7,
  // Model was in killswitch list of versions to be removed.
  kModelInKillSwitchList = 8,
  // Old model was removed due to new model update.
  kNewModelUpdate = 9,
  // Model dir was inconsistent with local_state and removed at startup.
  kInconsistentModelDir = 10,

  // Add new values above this line.
  kMaxValue = kInconsistentModelDir,
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_ENUMS_H_
