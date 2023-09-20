// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_FETCHER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_FETCHER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "url/gurl.h"

class OptimizationGuideLogger;

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace optimization_guide {

using ModelExecuteResponseCallback =
    base::OnceCallback<void(base::optional_ref<const proto::ExecuteResponse>)>;

class ModelExecutionFetcher {
 public:
  ModelExecutionFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& optimization_guide_service_url,
      OptimizationGuideLogger* optimization_guide_logger);

  ModelExecutionFetcher(const ModelExecutionFetcher&) = delete;
  ModelExecutionFetcher& operator=(const ModelExecutionFetcher&) = delete;

  ~ModelExecutionFetcher();

  void ExecuteModel(proto::ModelExecutionFeature feature,
                    const google::protobuf::MessageLite& request_metadata,
                    ModelExecuteResponseCallback callback);

 private:
  // URL loader completion callback.
  void OnURLLoadComplete(std::unique_ptr<std::string> response_body);

  // The URL for the remote Optimization Guide Service.
  const GURL optimization_guide_service_url_;

  // Used to hold the callback while the SimpleURLLoader performs the request
  // asynchronously.
  ModelExecuteResponseCallback model_execution_callback_;

  // Holds the currently active url request.
  std::unique_ptr<network::SimpleURLLoader> active_url_loader_;

  // Used for creating an `active_url_loader_` when needed for request hints.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The start time of the current fetch, used to determine the latency in
  // retrieving from the remote Optimization Guide Service.
  base::TimeTicks fetch_start_time_;

  // The model execution feature the fetch is happening for.
  proto::ModelExecutionFeature model_execution_feature_ =
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_UNSPECIFIED;

  // Owned by OptimizationGuideKeyedService and outlives `this`.
  raw_ptr<OptimizationGuideLogger> optimization_guide_logger_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_FETCHER_H_
