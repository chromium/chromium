// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_INPUT_DELEGATE_SHOPPING_SERVICE_INPUT_DELEGATE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_INPUT_DELEGATE_SHOPPING_SERVICE_INPUT_DELEGATE_H_

#include "components/commerce/core/shopping_service.h"
#include "components/segmentation_platform/public/input_delegate.h"

namespace segmentation_platform {

using commerce::ShoppingService;
using processing::FeatureProcessorState;

class ShoppingServiceInputDelegate : public processing::InputDelegate {
 public:
  explicit ShoppingServiceInputDelegate(
      base::RepeatingCallback<ShoppingService*()> shopping_service_callback);
  ~ShoppingServiceInputDelegate() override;

  // InputDelegate impl
  void Process(const proto::CustomInput& input,
               FeatureProcessorState& feature_processor_state,
               ProcessedCallback callback) override;

 private:
  base::RepeatingCallback<ShoppingService*()> shopping_service_callback_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_INPUT_DELEGATE_SHOPPING_SERVICE_INPUT_DELEGATE_H_
