// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MODEL_FETCHER_IMPL_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MODEL_FETCHER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "components/optimization_guide/core/prediction_model_fetcher.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace optimization_guide {

// A class to handle requests for prediction models (and prediction data) from
// a remote Optimization Guide Service.
//
// This class fetches new models from the remote Optimization Guide Service.
class PredictionModelFetcherImpl : public PredictionModelFetcher {
 public:
  PredictionModelFetcherImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& optimization_guide_service_get_models_url);

  PredictionModelFetcherImpl(const PredictionModelFetcherImpl&) = delete;
  PredictionModelFetcherImpl& operator=(const PredictionModelFetcherImpl&) =
      delete;

  ~PredictionModelFetcherImpl() override;

  // PredictionModelFetcher implementation
  bool FetchOptimizationGuideServiceModels(
      const std::vector<proto::ModelInfo>& models_request_info,
      proto::RequestContext request_context,
      const std::string& locale,
      ModelsFetchedCallback models_fetched_callback) override;

 private:
  // URL loader completion callback.
  void OnURLLoadComplete(std::unique_ptr<std::string> response_body);

  // Handles the response from the remote Optimization Guide Service.
  // |response| is the response body, |status| is the
  // |net::Error| of the response, and response_code is the HTTP
  // response code (if available).
  void HandleResponse(const std::string& response,
                      int status,
                      int response_code);

  // Used to hold the callback while the SimpleURLLoader performs the request
  // asynchronously.
  ModelsFetchedCallback models_fetched_callback_;

  // The URL for the remote Optimization Guide Service that serves models and
  // host features.
  const GURL optimization_guide_service_get_models_url_;

  // Used to hold the GetModelsRequest being constructed and sent as a remote
  // request.
  std::unique_ptr<optimization_guide::proto::GetModelsRequest>
      pending_models_request_;

  // Holds the URLLoader for an active hints request.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // Used for creating a |url_loader_| when needed for request hints.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MODEL_FETCHER_IMPL_H_
