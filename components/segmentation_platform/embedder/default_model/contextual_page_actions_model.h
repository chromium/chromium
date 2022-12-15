// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_CONTEXTUAL_PAGE_ACTIONS_MODEL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_CONTEXTUAL_PAGE_ACTIONS_MODEL_H_

#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

// Model provider for contextual page actions model that includes a default
// model and metadata.
class ContextualPageActionsModel : public ModelProvider {
 public:
  ContextualPageActionsModel();
  ~ContextualPageActionsModel() override = default;

  ContextualPageActionsModel(const ContextualPageActionsModel&) = delete;
  ContextualPageActionsModel& operator=(const ContextualPageActionsModel&) =
      delete;

  // ModelProvider implementation.
  void InitAndFetchModel(
      const ModelUpdatedCallback& model_updated_callback) override;
  void ExecuteModelWithInput(const ModelProvider::Request& inputs,
                             ExecutionCallback callback) override;
  bool ModelAvailable() override;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_CONTEXTUAL_PAGE_ACTIONS_MODEL_H_
