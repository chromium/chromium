// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PERMISSIONS_AIV3_ENCODER_H_
#define COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PERMISSIONS_AIV3_ENCODER_H_

#include <optional>
#include <vector>

#include "components/optimization_guide/core/base_model_executor.h"
#include "components/optimization_guide/core/model_executor.h"
#include "components/permissions/permission_request_enums.h"
#include "components/permissions/prediction_service/prediction_service_messages.pb.h"
#include "components/permissions/request_type.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace permissions {

// The executor maps its inputs into TFLite's tensor format and converts the
// model output's tensor representation back.
class PermissionsAiv3Encoder
    : public optimization_guide::BaseModelExecutor<PermissionRequestRelevance,
                                                   const SkBitmap&> {
 public:
  using ModelInput = SkBitmap;
  using ModelOutput = PermissionRequestRelevance;

  static const int kModelInputWidth;
  static const int kModelInputHeight;
  explicit PermissionsAiv3Encoder(RequestType request_type)
      : request_type_(request_type) {}
  ~PermissionsAiv3Encoder() override = default;

 protected:
  // optimization_guide::BaseModelEncoder:
  bool Preprocess(const std::vector<TfLiteTensor*>& input_tensors,
                  const ModelInput& input) override;
  std::optional<ModelOutput> Postprocess(
      const std::vector<const TfLiteTensor*>& output_tensors) override;

 private:
  RequestType request_type_;
};
}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PERMISSIONS_AIV3_ENCODER_H_
