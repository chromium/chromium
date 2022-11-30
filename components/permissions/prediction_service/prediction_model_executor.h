// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_MODEL_EXECUTOR_H_
#define COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_MODEL_EXECUTOR_H_

#include <vector>

#include "components/optimization_guide/core/base_model_executor.h"
#include "components/permissions/prediction_service/prediction_request_features.h"
#include "components/permissions/prediction_service/prediction_service_messages.pb.h"

namespace permissions {

class PredictionModelExecutor : public optimization_guide::BaseModelExecutor<
                                    GeneratePredictionsResponse,
                                    const GeneratePredictionsRequest&> {
 public:
  PredictionModelExecutor();
  ~PredictionModelExecutor() override;

  PredictionModelExecutor(const PredictionModelExecutor&) = delete;
  PredictionModelExecutor& operator=(const PredictionModelExecutor&) = delete;

 protected:
  // optimization_guide::BaseModelExecutor:
  bool Preprocess(const std::vector<TfLiteTensor*>& input_tensors,
                  const GeneratePredictionsRequest& input) override;

  absl::optional<GeneratePredictionsResponse> Postprocess(
      const std::vector<const TfLiteTensor*>& output_tensors) override;

 private:
  RequestType request_type_;
};

}  // namespace permissions
#endif  // COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_MODEL_EXECUTOR_H_
