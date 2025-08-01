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

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_ENUMS_H_
