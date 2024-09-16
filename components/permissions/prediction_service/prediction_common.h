// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_COMMON_H_
#define COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_COMMON_H_

#include "components/permissions/prediction_service/prediction_request_features.h"
#include "components/permissions/prediction_service/prediction_service_messages.pb.h"

namespace permissions {

constexpr char kDefaultPredictionServiceUrl[] =
    "https://webpermissionpredictions.googleapis.com/v1:generatePredictions";

// A command line switch to override the default service url.
constexpr char kDefaultPredictionServiceUrlSwitchKey[] =
    "permission-predictions-service-url";

constexpr float kRoundToMultiplesOf = 0.1f;

constexpr int kCountBuckets[] = {20, 15, 12, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};

// Thresholds of the likelihood that triggers the CPSS prompts.
constexpr float kNotificationPredictionsThreshold = 0.81;
constexpr float kGeolocationPredictionsThreshold = 0.95;

// Returns the ratio rounded to the nearest 10%. It returns a value between 0
// and 1 in steps of 0.1
float GetRoundedRatio(int numerator, int denominator);

// This method normalises the value returned by GetRoundedRatio(int, int) for
// sending it to ukm. It returns a value between 0 and 100 in steps of 10.
int GetRoundedRatioForUkm(int numerator, int denominator);

// Returns the appropriate bucket for `count`.
int BucketizeValue(int count);

// Get the current platform for proto message purposes.
ClientFeatures_Platform GetCurrentPlatformProto();

// Get the current platform for proto message purposes.
ClientFeatures_PlatformEnum GetCurrentPlatformEnumProto();

// Convert PermissionRequestGestureType to ClientFeatures_Gesture.
ClientFeatures_Gesture ConvertToProtoGesture(
    const permissions::PermissionRequestGestureType type);

// Convert PermissionRequestGestureType to ClientFeatures_GestureEnum.
ClientFeatures_GestureEnum ConvertToProtoGestureEnum(
    const permissions::PermissionRequestGestureType type);

// Fill in the values in StatsFeature using the values in
// PredictionRequestFeatures::ActionCounts
void FillInStatsFeatures(const PredictionRequestFeatures::ActionCounts& counts,
                         StatsFeatures* features);

std::unique_ptr<GeneratePredictionsRequest> GetPredictionRequestProto(
    const PredictionRequestFeatures& entity);

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_COMMON_H_
