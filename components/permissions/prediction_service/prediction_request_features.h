// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_REQUEST_FEATURES_H_
#define COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_REQUEST_FEATURES_H_

#include "components/permissions/permission_request_enums.h"

namespace permissions {

struct PredictionRequestFeatures {
  struct ActionCounts {
    unsigned int grants;
    unsigned int denies;
    unsigned int dismissals;
    unsigned int ignores;
  };

  // Whether a gesture is present or not.
  PermissionRequestGestureType gesture;

  // Which permissions request type this is for.
  PermissionRequestType type;

  // The permission action counts for this specific permission type.
  ActionCounts requested_permission_counts;

  // The permission action counts for all permissions type.
  ActionCounts all_permission_counts;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_REQUEST_FEATURES_H_
