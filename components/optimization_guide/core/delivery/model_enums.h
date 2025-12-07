// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_MODEL_ENUMS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_MODEL_ENUMS_H_

namespace optimization_guide {

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

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_MODEL_ENUMS_H_
