// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_ENUMS_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_ENUMS_H_

namespace permissions {

// Used for UMA to record whether a gesture was associated with the request. For
// simplicity not all request types track whether a gesture is associated with
// it or not, for these types of requests metrics are not recorded.
enum class PermissionRequestGestureType {
  UNKNOWN,
  GESTURE,
  NO_GESTURE,
  // NUM must be the last value in the enum.
  NUM
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(PermissionRequestRelevance)
enum class PermissionRequestRelevance {
  kUnspecified = 0,
  kVeryLow = 1,
  kLow = 2,
  kMedium = 3,
  kHigh = 4,
  kVeryHigh = 5,

  // Always keep at the end.
  kMaxValue = kVeryHigh,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/permissions/enums.xml:PermissionRequestRelevance)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(PermissionRequestLikelihood)
enum class PermissionRequestLikelihood {
  kUnspecified = 0,
  kVeryUnlikely = 1,
  kUnlikely = 2,
  kNeutral = 3,
  kLikely = 4,
  kVeryLikely = 5,

  // Always keep at the end.
  kMaxValue = kVeryLikely,
};
// LINT.ThenChange(
// tools/metrics/histograms/metadata/permissions/enums.xml:PermissionRequestLikelihood)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(PermissionAiRelevanceModel)
enum class PermissionAiRelevanceModel {
  kUnknown = 0,
  kAIv3 = 1,
  kAIv4 = 2,
  kMaxValue = kAIv4,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/permissions/enums.xml:PermissionAiRelevanceModel)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(PermissionPredictionSource)
enum class PermissionPredictionSource {
  kNoCpssModel = 0,
  kOnDeviceCpssV1Model = 1,
  kServerSideCpssV3Model = 2,
  kOnDeviceAiv1AndServerSideModel = 3,
  kOnDeviceAiv3AndServerSideModel = 4,
  kOnDeviceAiv4AndServerSideModel = 5,

  // Always keep at the end.
  kMaxValue = kOnDeviceAiv4AndServerSideModel,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/permissions/enums.xml:PermissionPredictionSource)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Used for UMA histograms to record the status of the language detection
// required for permissions AIv4 model execution workflow.
// LINT.IfChange(LanguageDetectionStatus)
enum LanguageDetectionStatus {
  kNoResultDueToTimeout = 0,
  kImmediatelyAvailableEnglish = 1,
  kImmediatelyAvailableNotEnglish = 2,
  kDelayedDetectedEnglish = 3,
  kDelayedDetectedNotEnglish = 4,

  kMaxValue = kDelayedDetectedNotEnglish,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/permissions/enums.xml:LanguageDetectionStatus)

// Used for UMA histograms to record model execution stats for the different
// models we use for a permission prediction.
// LINT.IfChange(PredictionModelType)
enum class PredictionModelType {
  kUnknown = 0,
  kServerSideCpssV3Model = 1,
  kOnDeviceCpssV1Model = 2,
  kOnDeviceAiV1Model = 3,
  kOnDeviceAiV3Model = 4,
  kOnDeviceAiV4Model = 5,

  // Always keep at the end.
  kMaxValue = kOnDeviceAiV4Model,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/permissions/histograms.xml:PredictionModels)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(PermissionPredictionSupportedType)
enum class PermissionPredictionSupportedType {
  kNotifications = 0,
  kGeolocation = 1,

  // Always keep at the end.
  kMaxValue = kGeolocation,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/permissions/enums.xml:PermissionPredictionSupportedType)

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_ENUMS_H_
