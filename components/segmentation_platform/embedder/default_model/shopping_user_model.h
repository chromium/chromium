// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_SHOPPING_USER_MODEL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_SHOPPING_USER_MODEL_H_

#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

// Segmentation shopping user model provider. Provides a default model and
// metadata for the shopping user optimization target.
class ShoppingUserModel : public ModelProvider {
 public:
  ShoppingUserModel();
  ~ShoppingUserModel() override = default;

  // Disallow copy/assign.
  ShoppingUserModel(ShoppingUserModel&) = delete;
  ShoppingUserModel& operator=(ShoppingUserModel&) = delete;

  // ModelProvider implementation.
  void InitAndFetchModel(
      const ModelUpdatedCallback& model_updated_callback) override;
  void ExecuteModelWithInput(const std::vector<float>& inputs,
                             ExecutionCallback callback) override;
  bool ModelAvailable() override;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_SHOPPING_USER_MODEL_H_