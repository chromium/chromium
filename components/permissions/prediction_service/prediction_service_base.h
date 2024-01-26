// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_SERVICE_BASE_H_
#define COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_SERVICE_BASE_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/permissions/permission_request_enums.h"
#include "components/permissions/prediction_service/prediction_request_features.h"
#include "components/permissions/prediction_service/prediction_service_messages.pb.h"

namespace permissions {

// TODO(crbug.com/1138595, andypaicu): Refactor this class and
// RealTimeUrlLookupServiceBase to derive from the same base class instead of
// doing a bunch of duplicate work. Design doc:
// https://docs.google.com/document/d/11Gd4bMpuPiVOVNhgqkixZXfckFDzv921BHoZWTBIISc/edit#heading=h.lxxeltml3hwr
class PredictionServiceBase : public KeyedService {
 public:
  // TODO(crbug.com/1138595, andypaicu): once the above TODO is done, refactor
  // to use a struct to make the call sites more readable (for both callbacks).
  using LookupRequestCallback =
      base::OnceCallback<void(std::unique_ptr<GeneratePredictionsRequest>,
                              std::string)>;  // Access token.

  using LookupResponseCallback = base::OnceCallback<void(
      bool,  // Lookup successful.
      bool,  // Response from cache.
      const std::optional<GeneratePredictionsResponse>&)>;

  virtual void StartLookup(const PredictionRequestFeatures& entity,
                           LookupRequestCallback request_callback,
                           LookupResponseCallback response_callback) = 0;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_SERVICE_BASE_H_
