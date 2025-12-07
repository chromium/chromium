// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_FEDCM_USER_SEGMENT_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_FEDCM_USER_SEGMENT_H_

#include <memory>
#include <string>

#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

struct Config;

// Model to predict if a user uses FedCM features.
class FedCmUserModel : public DefaultModelProvider {
 public:
  enum Feature {
    kFeatureAccountsDialogShown,
    kFeatureRequestIdToken,
    kFeatureCancelReason,
    kFeatureIsSignInUser,
    kFeatureCount
  };

  FedCmUserModel();
  ~FedCmUserModel() override = default;

  FedCmUserModel(const FedCmUserModel&) = delete;
  FedCmUserModel& operator=(const FedCmUserModel&) = delete;

  static std::unique_ptr<Config> GetConfig();

  // ModelProvider implementation.
  std::unique_ptr<ModelConfig> GetModelConfig() override;

  void ExecuteModelWithInput(const ModelProvider::Request& inputs,
                             ExecutionCallback callback) override;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_FEDCM_USER_SEGMENT_H_
