// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PERMISSIONS_AIV4_ENCODER_H_
#define COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PERMISSIONS_AIV4_ENCODER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/types/optional_ref.h"
#include "components/optimization_guide/core/inference/base_model_executor.h"
#include "components/optimization_guide/core/inference/model_executor.h"
#include "components/passage_embeddings/passage_embeddings_types.h"
#include "components/permissions/permission_request_enums.h"
#include "components/permissions/prediction_service/permissions_ai_encoder_base.h"
#include "components/permissions/prediction_service/prediction_service_messages.pb.h"
#include "components/permissions/request_type.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace permissions {

struct PermissionsAiv4EncoderInput {
  PermissionsAiv4EncoderInput(
      SkBitmap snapshot,
      passage_embeddings::Embedding rendered_text_embedding);
  ~PermissionsAiv4EncoderInput();
  PermissionsAiv4EncoderInput(const PermissionsAiv4EncoderInput&);
  PermissionsAiv4EncoderInput(PermissionsAiv4EncoderInput&&);
  SkBitmap snapshot;
  passage_embeddings::Embedding inner_text_embedding;
};

// The executor maps its inputs into TFLite's tensor format and converts the
// model output's tensor representation back.
class PermissionsAiv4Encoder
    : public PermissionsAiEncoderBase<const PermissionsAiv4EncoderInput&> {
 public:
  using ModelInput = PermissionsAiv4EncoderInput;
  // This is the output size of the embeddings model we use to encode the
  // rendered_text input.
  static constexpr int kTextInputSize = 768;

  explicit PermissionsAiv4Encoder(RequestType request_type)
      : PermissionsAiEncoderBase(request_type) {}
  ~PermissionsAiv4Encoder() override = default;

 protected:
  void SetThresholdValues();

  // optimization_guide::BaseModelEncoder:
  bool Preprocess(const std::vector<TfLiteTensor*>& input_tensors,
                  const ModelInput& input) override;
};
}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PERMISSIONS_AIV4_ENCODER_H_
