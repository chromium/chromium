// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_MODEL_HANDLER_PROVIDER_H_
#define COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_MODEL_HANDLER_PROVIDER_H_

#include <memory>

#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/permissions/prediction_service/prediction_model_handler.h"
#include "components/permissions/request_type.h"

namespace permissions {
class PredictionModelHandlerProvider : public KeyedService {
 public:
  explicit PredictionModelHandlerProvider(
      optimization_guide::OptimizationGuideModelProvider* optimization_guide);
  ~PredictionModelHandlerProvider() override;
  PredictionModelHandlerProvider(const PredictionModelHandlerProvider&) =
      delete;
  PredictionModelHandlerProvider& operator=(
      const PredictionModelHandlerProvider&) = delete;

  PredictionModelHandler* GetPredictionModelHandler(RequestType request_type);

 private:
  std::unique_ptr<PredictionModelHandler>
      notification_prediction_model_handler_;
  std::unique_ptr<PredictionModelHandler> geolocation_prediction_model_handler_;
};
}  // namespace permissions
#endif  // COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_MODEL_HANDLER_PROVIDER_H_
