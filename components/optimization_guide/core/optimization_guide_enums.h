// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_ENUMS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_ENUMS_H_

namespace optimization_guide {

// The types of decisions that can be made for an optimization type.
//
// Keep in sync with OptimizationGuideOptimizationTypeDecision in enums.xml.
enum class OptimizationTypeDecision {
  kUnknown,
  // The optimization type was allowed for the page load by an optimization
  // filter for the type.
  kAllowedByOptimizationFilter,
  // The optimization type was not allowed for the page load by an optimization
  // filter for the type.
  kNotAllowedByOptimizationFilter,
  // An optimization filter for that type was on the device but was not loaded
  // in time to make a decision. There is no guarantee that had the filter been
  // loaded that the page load would have been allowed for the optimization
  // type.
  kHadOptimizationFilterButNotLoadedInTime,
  // The optimization type was allowed for the page load based on a hint.
  kAllowedByHint,
  // A hint that matched the page load was present but the optimization type was
  // not allowed to be applied.
  kNotAllowedByHint,
  // A hint was available but there was not a page hint within that hint that
  // matched the page load.
  kNoMatchingPageHint,
  // A hint that matched the page load was on the device but was not loaded in
  // time to make a decision. There is no guarantee that had the hint been
  // loaded that the page load would have been allowed for the optimization
  // type.
  kHadHintButNotLoadedInTime,
  // No hints were available in the cache that matched the page load.
  kNoHintAvailable,
  // The OptimizationGuideDecider was not initialized yet.
  kDeciderNotInitialized,
  // A fetch to get the hint for the page load from the remote Optimization
  // Guide Service was started, but was not available in time to make a
  // decision.
  kHintFetchStartedButNotAvailableInTime,

  // Add new values above this line.
  kMaxValue = kHintFetchStartedButNotAvailableInTime,
};

// The types of decisions that can be made for an optimization target.
//
// Keep in sync with OptimizationGuideOptimizationTargetDecision in enums.xml.
enum class OptimizationTargetDecision {
  kUnknown,
  // The page load does not match the optimization target.
  kPageLoadDoesNotMatch,
  // The page load matches the optimization target.
  kPageLoadMatches,
  // The model needed to make the target decision was not available on the
  // client.
  kModelNotAvailableOnClient,
  // The page load is part of a model prediction holdback where all decisions
  // will return |OptimizationGuideDecision::kFalse| in an attempt to not taint
  // the data for understanding the production recall of the model.
  kModelPredictionHoldback,
  // The OptimizationGuideDecider was not initialized yet.
  kDeciderNotInitialized,

  // Add new values above this line.
  kMaxValue = kDeciderNotInitialized,
};

// The statuses for racing a hints fetch with the current navigation based
// on the availability of hints for both the current host and URL.
//
// Keep in sync with OptimizationGuideRaceNavigationFetchAttemptStatus in
// enums.xml.
enum class RaceNavigationFetchAttemptStatus {
  kUnknown,
  // The race was not attempted because hint information for the host and URL
  // of the current navigation was already available.
  kRaceNavigationFetchNotAttempted,
  // The race was attempted for the host of the current navigation but not the
  // URL.
  kRaceNavigationFetchHost,
  // The race was attempted for the URL of the current navigation but not the
  // host.
  kRaceNavigationFetchURL,
  // The race was attempted for the host and URL of the current navigation.
  kRaceNavigationFetchHostAndURL,
  // A race for the current navigation's URL is already in progress.
  kRaceNavigationFetchAlreadyInProgress,
  // DEPRECATED: A race for the current navigation's URL was not attempted
  // because there were too many concurrent page navigation fetches in flight.
  kDeprecatedRaceNavigationFetchNotAttemptedTooManyConcurrentFetches,

  // Add new values above this line.
  kMaxValue =
      kDeprecatedRaceNavigationFetchNotAttemptedTooManyConcurrentFetches,
};

// The statuses for a prediction model in the prediction manager when requested
// to be evaluated.
//
// Keep in sync with OptimizationGuidePredictionManagerModelStatus in enums.xml.
enum class PredictionManagerModelStatus {
  kUnknown,
  // The model is loaded and available for use.
  kModelAvailable,
  // The store is initialized but does not contain a model for the optimization
  // target.
  kStoreAvailableNoModelForTarget,
  // The store is initialized and contains a model for the optimization target
  // but it is not loaded in memory.
  kStoreAvailableModelNotLoaded,
  // The store is not initialized and it is unknown if it contains a model for
  // the optimization target.
  kStoreUnavailableModelUnknown,

  // Add new values above this line.
  kMaxValue = kStoreUnavailableModelUnknown,
};

// The statuses for a download file containing a prediction model when verified
// and processed.
//
// Keep in sync with OptimizationGuidePredictionModelDownloadStatus
// in enums.xml.
enum class PredictionModelDownloadStatus {
  kUnknown,
  // The downloaded file was successfully verified and processed.
  kSuccess,
  // The downloaded file was not a valid CRX file.
  kFailedCrxVerification,
  // A temporary directory for unzipping the CRX file failed to be created.
  kFailedUnzipDirectoryCreation,
  // The CRX file failed to be unzipped.
  kFailedCrxUnzip,
  // The model info failed to be read from disk.
  kFailedModelInfoFileRead,
  // The model info failed to be parsed.
  kFailedModelInfoParsing,
  // The model file was not found in the CRX file.
  kFailedModelFileNotFound,
  // The model file failed to be moved to a more permanent directory.
  kFailedModelFileOtherError,
  // The model info was invalid.
  kFailedModelInfoInvalid,
  // The CRX file was a valid CRX file but did not come from a valid publisher.
  kFailedCrxInvalidPublisher,

  // Add new values above this line.
  kMaxValue = kFailedCrxInvalidPublisher,
};

// The state of the model file needed for execution.
//
// Keep in sync with ModelExecutorLoadingState in enums.xml.
enum class ModelExecutorLoadingState {
  // The model state is not known.
  kUnknown = 0,
  // The provided model file was not valid.
  kModelFileInvalid = 1,
  // The model is memory-mapped and available for
  // use with TFLite.
  kModelFileValidAndMemoryMapped = 2,

  // New values above this line.
  kMaxValue = kModelFileValidAndMemoryMapped,
};

// The status for the page content annotations being stored.
//
// Keep in sync with OptimizationGuidePageContentAnnotationsStorageStatus in
// enums.xml.
enum PageContentAnnotationsStorageStatus {
  kUnknown = 0,
  // The content annotations were requested to be stored in the History Service.
  kSuccess = 1,
  // There were no visits for the URL found in the History Service.
  kNoVisitsForUrl = 2,
  // The specific visit that we wanted to annotate could not be found in the
  // History Service.
  kSpecificVisitForUrlNotFound = 3,

  // Add new values above this line.
  kMaxValue = kSpecificVisitForUrlNotFound,
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_ENUMS_H_
