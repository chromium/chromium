// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/input_delegate/shopping_service_input_delegate.h"

namespace segmentation_platform {

using processing::ProcessedValue;
using processing::Tensor;

ShoppingServiceInputDelegate::ShoppingServiceInputDelegate(
    base::RepeatingCallback<ShoppingService*()> shopping_service_callback)
    : shopping_service_callback_(shopping_service_callback) {}

ShoppingServiceInputDelegate::~ShoppingServiceInputDelegate() = default;

void ShoppingServiceInputDelegate::Process(
    const proto::CustomInput& input,
    FeatureProcessorState& feature_processor_state,
    ProcessedCallback callback) {
  auto* shopping_service = shopping_service_callback_.Run();

  Tensor inputs(1, ProcessedValue(0.0f));

  if (!shopping_service) {
    std::move(callback).Run(/*error=*/true, std::move(inputs));
    return;
  }

  inputs[0] = ProcessedValue(
      static_cast<float>(shopping_service->GetAllShoppingBookmarks().size()));

  std::move(callback).Run(/*error=*/false, std::move(inputs));
}

}  // namespace segmentation_platform
