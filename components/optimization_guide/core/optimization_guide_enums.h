// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_ENUMS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_ENUMS_H_

namespace optimization_guide {

// The types of decisions that can be made for an optimization type.
//
// Keep in sync with OptimizationGuideOptimizationTypeDecision in enums.xml.
enum class OptimizationTypeDecision {
  kUnknown = 0,
  // The optimization type was allowed for the page load by an optimization
  // filter for the type.
  kAllowedByOptimizationFilter = 1,
  // The optimization type was not allowed for the page load by an optimization
  // filter for the type.
  kNotAllowedByOptimizationFilter = 2,
  // An optimization filter for that type was on the device but was not loaded
  // in time to make a decision. There is no guarantee that had the filter been
  // loaded that the page load would have been allowed for the optimization
  // type.
  kHadOptimizationFilterButNotLoadedInTime = 3,
  // The optimization type was allowed for the page load based on a hint.
  kAllowedByHint = 4,
  // A hint that matched the page load was present but the optimization type was
  // not allowed to be applied.
  kNotAllowedByHint = 5,
  // A hint was available but there was not a page hint within that hint that
  // matched the page load.
  kNoMatchingPageHint = 6,
  // A hint that matched the page load was on the device but was not loaded in
  // time to make a decision. There is no guarantee that had the hint been
  // loaded that the page load would have been allowed for the optimization
  // type.
  kHadHintButNotLoadedInTime = 7,
  // No hints were available in the cache that matched the page load.
  kNoHintAvailable = 8,
  // The OptimizationGuideDecider was not initialized yet.
  kDeciderNotInitialized = 9,
  // A fetch to get the hint for the page load from the remote Optimization
  // Guide Service was started, but was not available in time to make a
  // decision.
  kHintFetchStartedButNotAvailableInTime = 10,

  // Add new values above this line.
  kMaxValue = kHintFetchStartedButNotAvailableInTime,
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

// The statuses for a download file containing a prediction model when verified
// and processed.
//
// Keep in sync with OptimizationGuidePredictionModelDownloadStatus
// in enums.xml.
enum class PredictionModelDownloadStatus {
  kUnknown = 0,
  // The downloaded file was successfully verified and processed.
  kSuccess = 1,
  // The downloaded file was not a valid CRX file.
  kFailedCrxVerification = 2,
  // A temporary directory for unzipping the CRX file failed to be created.
  kFailedUnzipDirectoryCreation = 3,
  // The CRX file failed to be unzipped.
  kFailedCrxUnzip = 4,
  // The model info failed to be read from disk.
  kFailedModelInfoFileRead = 5,
  // The model info failed to be parsed.
  kFailedModelInfoParsing = 6,
  // The model file was not found in the CRX file.
  kFailedModelFileNotFound = 7,
  // The model file failed to be moved to a more permanent directory.
  kFailedModelFileOtherError = 8,
  // The model info was invalid.
  kFailedModelInfoInvalid = 9,
  // The CRX file was a valid CRX file but did not come from a valid publisher.
  kFailedCrxInvalidPublisher = 10,
  // The opt guide parent directory for storing models in does not exist.
  kOptGuideDirectoryDoesNotExist = 11,
  // The new directory to persist this model version's files could not be
  // created.
  kCouldNotCreateDirectory = 12,
  // The model info was not saved to model store file.
  kFailedModelInfoSaving = 13,

  // Add new values above this line.
  kMaxValue = kFailedModelInfoSaving,
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

// Different events of the prediction model delivery lifecycle for an
// OptimizationTarget.
// Keep in sync with OptimizationGuideModelDeliveryEvent in enums.xml.
enum class ModelDeliveryEvent {
  kUnknown = 0,

  // The model was delivered from immediately or after a
  // successful download.
  kModelDeliveredAtRegistration = 1,
  kModelDelivered = 2,

  // GetModelsRequest was sent to the optimization guide server.
  kGetModelsRequest = 3,

  // Model was requested to be downloaded using download service.
  kDownloadServiceRequest = 4,

  // Download service started the model download.
  kModelDownloadStarted = 5,

  // Model got downloaded from the download service.
  kModelDownloaded = 6,

  // Download service was unavailable.
  kDownloadServiceUnavailable = 7,

  // GetModelsResponse failed.
  kGetModelsResponseFailure = 8,

  // Download URL received from model metadata is invalid
  kDownloadURLInvalid = 9,

  // Model download failed due to download service or verifying the downloaded
  // model.
  kModelDownloadFailure = 10,

  // Add new values above this line.
  kMaxValue = kModelDownloadFailure,
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_ENUMS_H_
