// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MODEL_FETCHER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MODEL_FETCHER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace optimization_guide {

// Callback to inform the caller that the remote hints have been fetched and
// to pass back the fetched hints response from the remote Optimization Guide
// Service.
using ModelsFetchedCallback = base::OnceCallback<void(
    std::optional<
        std::unique_ptr<optimization_guide::proto::GetModelsResponse>>)>;

// A class to handle requests for prediction models (and prediction data) from
// a remote Optimization Guide Service.
//
// This class fetches new models from the remote Optimization Guide Service.
class PredictionModelFetcher {
 public:
  PredictionModelFetcher() = default;
  PredictionModelFetcher(const PredictionModelFetcher&) = delete;
  PredictionModelFetcher& operator=(const PredictionModelFetcher&) = delete;

  virtual ~PredictionModelFetcher() = default;

  // Requests PredictionModels and HostModelFeatures from the Optimization Guide
  // Service if a request for them is not already in progress. Returns whether a
  // new request was issued. |models_fetched_callback| is called when the
  // request is complete providing the GetModelsResponse object if successful or
  // nullopt if the fetch failed or no fetch is needed. Virtualized for testing.
  virtual bool FetchOptimizationGuideServiceModels(
      const std::vector<proto::ModelInfo>& models_request_info,
      proto::RequestContext request_context,
      const std::string& locale,
      ModelsFetchedCallback models_fetched_callback) = 0;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MODEL_FETCHER_H_
