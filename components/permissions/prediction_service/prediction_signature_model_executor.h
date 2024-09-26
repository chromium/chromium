// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_SIGNATURE_MODEL_EXECUTOR_H_
#define COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_SIGNATURE_MODEL_EXECUTOR_H_

#include "components/optimization_guide/core/signature_model_executor.h"
#include "components/permissions/prediction_service/prediction_model_executor.h"
#include "components/permissions/prediction_service/prediction_model_metadata.pb.h"
#include "components/permissions/prediction_service/prediction_service_messages.pb.h"
#include "components/permissions/request_type.h"

namespace permissions {

// Implements SignatureModelExecutor to execute models with mapped input and
// output TfLiteTensors. Input represents various permission action rates.
// Output is between 0 and 1, which represents the probability for the
// permission to be denied.
class PredictionSignatureModelExecutor
    : public optimization_guide::SignatureModelExecutor<
          GeneratePredictionsResponse,
          const PredictionModelExecutorInput&> {
 public:
  PredictionSignatureModelExecutor();
  ~PredictionSignatureModelExecutor() override;

  PredictionSignatureModelExecutor(const PredictionSignatureModelExecutor&) =
      delete;
  PredictionSignatureModelExecutor& operator=(
      const PredictionSignatureModelExecutor&) = delete;

 protected:
  // optimization_guide::SignatureModelExecutor
  bool Preprocess(const std::map<std::string, TfLiteTensor*>& input_tensors_map,
                  const PredictionModelExecutorInput& input) override;

  std::optional<GeneratePredictionsResponse> Postprocess(
      const std::map<std::string, const TfLiteTensor*>& output_tensors_map)
      override;

  const char* GetSignature() override;

 private:
  RequestType request_type_;
  std::optional<WebPermissionPredictionsModelMetadata> model_metadata_;
};

}  // namespace permissions
#endif  // COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_SIGNATURE_MODEL_EXECUTOR_H_
