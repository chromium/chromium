// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PERMISSIONS_AIV3_ENCODER_H_
#define COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PERMISSIONS_AIV3_ENCODER_H_

#include <optional>
#include <vector>

#include "base/types/optional_ref.h"
#include "components/optimization_guide/core/inference/base_model_executor.h"
#include "components/optimization_guide/core/inference/model_executor.h"
#include "components/permissions/permission_request_enums.h"
#include "components/permissions/prediction_service/permissions_ai_encoder_base.h"
#include "components/permissions/prediction_service/permissions_aiv3_model_metadata.pb.h"
#include "components/permissions/prediction_service/prediction_service_messages.pb.h"
#include "components/permissions/request_type.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace permissions {

struct PermissionsAiv3EncoderInput {
  explicit PermissionsAiv3EncoderInput(SkBitmap snapshot);
  PermissionsAiv3EncoderInput();
  ~PermissionsAiv3EncoderInput();
  PermissionsAiv3EncoderInput(const PermissionsAiv3EncoderInput&);
  PermissionsAiv3EncoderInput(PermissionsAiv3EncoderInput&&);
  SkBitmap snapshot;
  std::optional<PermissionsAiv3ModelMetadata> metadata;
};

// The executor maps its inputs into TFLite's tensor format and converts the
// model output's tensor representation back.
class PermissionsAiv3Encoder
    : public PermissionsAiEncoderBase<const PermissionsAiv3EncoderInput&> {
 public:
  using ModelInput = PermissionsAiv3EncoderInput;

  explicit PermissionsAiv3Encoder(RequestType request_type)
      : PermissionsAiEncoderBase(request_type) {}
  ~PermissionsAiv3Encoder() override = default;

  void SetThresholdsFromMetadata(
      std::optional<PermissionsAiv3ModelMetadata>& metadata);

 protected:
  // optimization_guide::BaseModelEncoder:
  bool Preprocess(const std::vector<TfLiteTensor*>& input_tensors,
                  const ModelInput& input) override;

 private:
  void SetThresholdValues(
      base::optional_ref<const PermissionsAiv3ModelMetadata> metadata);
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PERMISSIONS_AIV3_ENCODER_H_
