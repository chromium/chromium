// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_SERVICE_COMMON_H_
#define COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_SERVICE_COMMON_H_

#include "components/permissions/prediction_service/prediction_service_messages.pb.h"

namespace permissions {

// TODO(andypaicu): when available, replace with actual URL.
constexpr char kDefaultPredictionServiceUrl[] =
    "https://webpermissionpredictions.googleapis.com/v1:generatePredictions";

// A command line switch to override the default service url.
constexpr char kDefaultPredictionServiceUrlSwitchKey[] =
    "permission-predictions-service-url";

// Get the current platform for proto message purposes.
ClientFeatures_Platform GetCurrentPlatformProto();

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_SERVICE_COMMON_H_
