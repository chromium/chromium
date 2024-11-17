// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_MODEL_EXECUTOR_H_
#define COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_MODEL_EXECUTOR_H_

#include <vector>

#include "components/optimization_guide/core/base_model_executor.h"
#include "components/permissions/prediction_service/prediction_model_metadata.pb.h"
#include "components/permissions/prediction_service/prediction_request_features.h"
#include "components/permissions/prediction_service/prediction_service_messages.pb.h"

namespace permissions {

// This enum backs up the 'PermissionPredictionThresholdSource` histogram
// enum.
// It indicates whether the prediction score threshold value obtained from the
// model or if it used the default fallback value.
// The enum is used for histograms, do not reorder or renumber the entries.
enum class PermissionPredictionThresholdSource {
  MODEL_METADATA = 0,
  HARDCODED_FALLBACK = 1,

  // Always keep at the end.
  kMaxValue = HARDCODED_FALLBACK,
};

struct PredictionModelExecutorInput {
  PredictionModelExecutorInput();
  ~PredictionModelExecutorInput();
  PredictionModelExecutorInput(const PredictionModelExecutorInput&);

  GeneratePredictionsRequest request;
  std::optional<WebPermissionPredictionsModelMetadata> metadata;
};

class PredictionModelExecutor : public optimization_guide::BaseModelExecutor<
                                    GeneratePredictionsResponse,
                                    const PredictionModelExecutorInput&> {
 public:
  PredictionModelExecutor();
  ~PredictionModelExecutor() override;

  PredictionModelExecutor(const PredictionModelExecutor&) = delete;
  PredictionModelExecutor& operator=(const PredictionModelExecutor&) = delete;

 protected:
  // optimization_guide::BaseModelExecutor:
  bool Preprocess(const std::vector<TfLiteTensor*>& input_tensors,
                  const PredictionModelExecutorInput& input) override;

  std::optional<GeneratePredictionsResponse> Postprocess(
      const std::vector<const TfLiteTensor*>& output_tensors) override;

 private:
  RequestType request_type_;
  std::optional<WebPermissionPredictionsModelMetadata> model_metadata_;
};

}  // namespace permissions
#endif  // COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_MODEL_EXECUTOR_H_
