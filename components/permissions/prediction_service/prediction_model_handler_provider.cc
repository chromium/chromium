// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/prediction_service/prediction_model_handler_provider.h"

#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/permissions/prediction_service/prediction_model_handler.h"
#include "components/permissions/request_type.h"

namespace permissions {

PredictionModelHandlerProvider::PredictionModelHandlerProvider(
    optimization_guide::OptimizationGuideModelProvider* optimization_guide) {
  notification_prediction_model_handler_ = new PredictionModelHandler(
      optimization_guide,
      optimization_guide::proto::OptimizationTarget::
          OPTIMIZATION_TARGET_NOTIFICATION_PERMISSION_PREDICTIONS);

  geolocation_prediction_model_handler_ = new PredictionModelHandler(
      optimization_guide,
      optimization_guide::proto::OptimizationTarget::
          OPTIMIZATION_TARGET_GEOLOCATION_PERMISSION_PREDICTIONS);
}
PredictionModelHandler*
PredictionModelHandlerProvider::GetPredictionModelHandler(
    RequestType request_type) {
  switch (request_type) {
    case RequestType::kNotifications:
      return notification_prediction_model_handler_;
    case RequestType::kGeolocation:
      return geolocation_prediction_model_handler_;
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace permissions
