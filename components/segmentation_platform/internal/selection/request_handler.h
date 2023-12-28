// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_REQUEST_HANDLER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_REQUEST_HANDLER_H_

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "components/segmentation_platform/internal/data_collection/training_data_collector.h"
#include "components/segmentation_platform/internal/scheduler/execution_service.h"
#include "components/segmentation_platform/internal/selection/segment_result_provider.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/proto/prediction_result.pb.h"
#include "components/segmentation_platform/public/result.h"

namespace segmentation_platform {
struct Config;
struct PredictionOptions;

// RequestHandler handles client API requests for a single client. Internally,
// it invokes the result provider for getting raw results and converts them to
// the postprocessed results as required by the classification/regression API.
// Only used for on-demand executions.
class RequestHandler {
 public:
  RequestHandler() = default;
  virtual ~RequestHandler() = default;

  // Creates the instance.
  static std::unique_ptr<RequestHandler> Create(
      const Config& config,
      std::unique_ptr<SegmentResultProvider> result_provider,
      ExecutionService* execution_service,
      StorageService* storage_service);

  // Fetches raw result for on demand executions.
  virtual void GetPredictionResult(const PredictionOptions& options,
                                   scoped_refptr<InputContext> input_context,
                                   RawResultCallback callback) = 0;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_REQUEST_HANDLER_H_
