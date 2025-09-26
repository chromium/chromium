// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PERMISSIONS_AI_ENCODER_BASE_H_
#define COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PERMISSIONS_AI_ENCODER_BASE_H_

#include <optional>
#include <vector>

#include "base/types/optional_ref.h"
#include "components/optimization_guide/core/inference/base_model_executor.h"
#include "components/optimization_guide/core/inference/model_executor.h"
#include "components/permissions/permission_request_enums.h"
#include "components/permissions/request_type.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace permissions {

// The executor maps its inputs into TFLite's tensor format and converts the
// model output's tensor representation back.
template <typename EncoderInput>
class PermissionsAiEncoderBase
    : public optimization_guide::BaseModelExecutor<PermissionRequestRelevance,
                                                   const EncoderInput&> {
 public:
  static const int kImageInputWidth;
  static const int kImageInputHeight;

  using ModelInput = EncoderInput;
  using ModelOutput = PermissionRequestRelevance;

  explicit PermissionsAiEncoderBase(RequestType request_type)
      : request_type_(request_type) {}
  ~PermissionsAiEncoderBase() override = default;

 protected:
  // optimization_guide::BaseModelEncoder:
  bool Preprocess(const std::vector<TfLiteTensor*>& input_tensors,
                  const ModelInput& input) override = 0;
  std::optional<ModelOutput> Postprocess(
      const std::vector<const TfLiteTensor*>& output_tensors) override;

  RequestType request_type() const { return request_type_; }
  std::array<float, 4>& relevance_thresholds() { return relevance_thresholds_; }

  bool ConvertSkBitMapToTfliteTensor(TfLiteTensor* input_tensor,
                                     const SkBitmap& input);

 private:
  RequestType request_type_;
  std::array<float, 4> relevance_thresholds_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PERMISSIONS_AI_ENCODER_BASE_H_
