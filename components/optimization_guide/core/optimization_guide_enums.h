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
  // A fetch to get the hint for the page load from the remote Optimization
  // Guide Service was started, but requested optimization type was not
  // registered.
  kRequestedUnregisteredType = 11,
  // A fetch to get the hint for the page load from the remote Optimization
  // Guide Service was started, but requested URL was invalid.
  kInvalidURL = 12,

  // Add new values above this line.
  kMaxValue = kInvalidURL,
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
  // The additional file was not found in the CRX file.
  kFailedInvalidAdditionalFile = 14,

  // Add new values above this line.
  kMaxValue = kFailedInvalidAdditionalFile,
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

  // Loading the model from store failed.
  kModelLoadFailed = 11,

  // Model download was attempted after the model load failed.
  kModelDownloadDueToModelLoadFailure = 12,

  // Add new values above this line.
  kMaxValue = kModelDownloadDueToModelLoadFailure,
};

// The various model quality user feedback.
enum class ModelQualityUserFeedback {
  kUnknown = 0,
  kThumbsDown = 1,
  kThumbsUp = 2,

  // Keep in sync with OptimizationGuideUserFeedback in
  // tools/metrics/histograms/metadata/optimization/enums.xml.
  kMaxValue = kThumbsUp,
};

// The various results of an access token request.
//
// Keep in sync with OptimizationGuideAccessTokenResult in enums.xml.
enum class OptimizationGuideAccessTokenResult {
  kUnknown = 0,
  // The access token was received successfully.
  kSuccess = 1,
  // User was not signed-in.
  kUserNotSignedIn = 2,
  // Failed with a transient error.
  kTransientError = 3,
  // Failed with a persistent error.
  kPersistentError = 4,

  // Add new values above this line.
  kMaxValue = kPersistentError,
};

// Status of a request to fetch from the optimization guide service.
// This enum must remain synchronized with the enum
// |OptimizationGuideFetcherRequestStatus| in
// tools/metrics/histograms/enums.xml.
enum class FetcherRequestStatus {
  // No fetch status known. Used in testing.
  kUnknown,
  // Fetch request was sent and a response received.
  kSuccess,
  // Fetch request was sent but no response received.
  kResponseError,
  // DEPRECATED: Fetch request not sent because of offline network status.
  kDeprecatedNetworkOffline,
  // Fetch request not sent because fetcher was busy with another request.
  kFetcherBusy,
  // Hints fetch request not sent because the host and URL lists were empty.
  kNoHostsOrURLsToFetchHints,
  // Hints fetch request not sent because no supported optimization types were
  // provided.
  kNoSupportedOptimizationTypesToFetchHints,
  // Fetch request was canceled before completion.
  kRequestCanceled,
  // Fetch request was not started because user was not signed-in.
  kUserNotSignedIn,

  // Insert new values before this line.
  kMaxValue = kUserNotSignedIn
};

// Status of the on-device model.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class OnDeviceModelStatus {
  // Model is installed and ready to use.
  kReady = 0,
  // Criteria to install model have not been met.
  kNotEligible = 1,
  // Criteria to install are met, but model not installed yet.
  kInstallNotComplete = 2,
  // The model installer was not registered, even though the client would be
  // eligible to install right now. This likely means the state of the system
  // has changed recently.
  kModelInstallerNotRegisteredForUnknownReason = 3,
  // The model is ready, but it wasn't ready early enough for
  // OnDeviceModelServiceController to use it.
  kModelInstalledTooLate = 4,
  // The model is not ready, and the reason is unknown.
  kNotReadyForUnknownReason = 5,

  // This must be kept in sync with
  // OptimizationGuideOnDeviceModelStatus in optimization/enums.xml.

  // Insert new values before this line.
  kMaxValue = kNotReadyForUnknownReason,
};

// Status of a model quality logs upload request.
enum class ModelQualityLogsUploadStatus {
  kUnknown = 0,
  // Logs upload was successful.
  kUploadSuccessful = 1,
  // Upload is disabled due to logging feature not enabled.
  kLoggingNotEnabled = 2,
  // Upload was not successful because of network error.
  kNetError = 3,
  // Upload is disabled due to metrics reporting being disabled in
  // chrome://settings.
  kMetricsReportingDisabled = 4,
  // Upload is disabled due to enterprise policy.
  kDisabledDueToEnterprisePolicy = 5,
  // Upload is disabled because the feature is not enabled for the user.
  kFeatureNotEnabledForUser = 6,

  // Insert new values before this line.
  // This enum must remain synchronized with the enum
  // |OptimizationGuideModelQualityLogsUploadStatus| in
  // tools/metrics/histograms/metadata/optimization/enums.xml.
  kMaxValue = kFeatureNotEnabledForUser,
};

// Performance class of this device.
//
// These values are persisted to logs and prefs. Entries should not be
// renumbered and numeric values should never be reused.
enum class OnDeviceModelPerformanceClass : int {
  kUnknown = 0,

  // See on_device_model::mojom::PerformanceClass for explanation of these.
  kError = 1,
  kVeryLow = 2,
  kLow = 3,
  kMedium = 4,
  kHigh = 5,
  kVeryHigh = 6,

  // WARNING!: If you add a new performance class, please be aware of
  // `IsPerformanceClassCompatibleWithOnDeviceModel`.

  // The service crashed, so a valid value was not returned.
  kServiceCrash = 7,

  // GPU was blocklisted.
  kGpuBlocked = 8,

  // Native library failed to load.
  kFailedToLoadLibrary = 9,

  // This must be kept in sync with
  // OnDeviceModelPerformanceClass in optimization/enums.xml.

  // Insert new values before this line.
  kMaxValue = kFailedToLoadLibrary,
};

// The result of loading an on-device model.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class OnDeviceModelLoadResult {
  kUnknown = 0,

  // Model loaded successfully.
  kSuccess = 1,

  // GPU was blocklisted.
  kGpuBlocked = 2,

  // Native library failed to load.
  kFailedToLoadLibrary = 3,

  // This must be kept in sync with
  // OnDeviceModelLoadResult in optimization/enums.xml.

  // Insert new values before this line.
  kMaxValue = kFailedToLoadLibrary,
};

// The validity of the model metadata packaged with the text safety model.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class TextSafetyModelMetadataValidity {
  kUnknown = 0,

  // No metadata packaged with model.
  kNoMetadata = 1,

  // Metadata packaged with model is of the wrong type.
  kMetadataWrongType = 2,

  // Metadata packaged with model has no feature configs.
  kNoFeatureConfigs = 3,

  // Metadata was valid.
  kValid = 4,

  // This must be kept in sync with TextSafetyModelMetadataValidity in
  // optimization/enums.xml.

  kMaxValue = kValid,
};

// Enumerates the different reasons for model remote disconnection.
enum class ModelRemoteDisconnectReason {
  kDisconncted,
  kRemoteIdle,

  kGpuBlocked,
  kModelLoadFailed,
};

enum class OnDeviceModelAdaptationAvailability {
  // Adaptation model was available.
  kAvailable = 0,

  // Base model was not available.
  kBaseModelUnavailable = 1,

  // Base model spec was invalid, so adaptation model cannot be fetched.
  kBaseModelSpecInvalid = 2,

  // Adaptation model was not available.
  kAdaptationModelUnavailable = 3,

  // The received adaptation model was invalid.
  kAdaptationModelInvalid = 4,

  // The received adaptation model was incompatible with the base model.
  kAdaptationModelIncompatible = 5,

  // The execution config in the adaptation model was invalid.
  kAdaptationModelExecutionConfigInvalid = 6,

  // The model execution feature was not recently used.
  kFeatureNotRecentlyUsed = 7,

  // This must be kept in sync with OnDeviceModelAdaptationAvailability in
  // optimization/enums.xml.
  kMaxValue = kFeatureNotRecentlyUsed,
};

// The result of running validation prompts for the on-device model.
//
// Keep in sync with OnDeviceModelValidationResult in enums.xml.
enum class OnDeviceModelValidationResult {
  kUnknown = 0,
  // The validation is currently running or was interrupted.
  kPending = 1,
  // The validation test succeeded.
  kSuccess = 2,
  // The validation test produced non-matching output.
  kNonMatchingOutput = 3,
  // The service crashed while running the validation test.
  kServiceCrash = 4,
  // The validation test was interrupted by another session.
  kInterrupted = 5,

  // This must be kept in sync with OnDeviceModelValidationResult in
  // optimization/enums.xml.
  kMaxValue = kInterrupted,
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_ENUMS_H_
