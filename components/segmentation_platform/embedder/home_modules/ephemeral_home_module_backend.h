// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_EPHEMERAL_HOME_MODULE_BACKEND_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_EPHEMERAL_HOME_MODULE_BACKEND_H_

#include <memory>

#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"
#include "components/segmentation_platform/internal/metadata/feature_query.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform::home_modules {

// Model to predict whether the user belongs to EphemeralHomeModuleBackend
// segment.
class EphemeralHomeModuleBackend : public DefaultModelProvider {
 public:
  explicit EphemeralHomeModuleBackend(
      base::WeakPtr<HomeModulesCardRegistry> home_modules_card_registry);
  ~EphemeralHomeModuleBackend() override;

  EphemeralHomeModuleBackend(const EphemeralHomeModuleBackend&) = delete;
  EphemeralHomeModuleBackend& operator=(const EphemeralHomeModuleBackend&) =
      delete;

  static std::unique_ptr<Config> GetConfig(
      HomeModulesCardRegistry* home_modules_card_registry);

  // ModelProvider implementation.
  std::unique_ptr<ModelConfig> GetModelConfig() override;
  void ExecuteModelWithInput(const ModelProvider::Request& inputs,
                             ExecutionCallback callback) override;

  void set_home_modules_card_registry_for_testing(
      HomeModulesCardRegistry* home_modules_card_registry) {
    home_modules_card_registry_ = home_modules_card_registry->GetWeakPtr();
  }

 private:
  base::WeakPtr<HomeModulesCardRegistry> home_modules_card_registry_;
};

// Test model for EphemeralHomeModuleBackend that uses same config, but just
// gives the card passed through the 'kEphemeralModuleBackendRankerTestOverride'
// commandline arg a top score.
class TestEphemeralHomeModuleBackend : public DefaultModelProvider {
 public:
  TestEphemeralHomeModuleBackend();
  ~TestEphemeralHomeModuleBackend() override = default;

  TestEphemeralHomeModuleBackend(const TestEphemeralHomeModuleBackend&) =
      delete;
  TestEphemeralHomeModuleBackend& operator=(
      const TestEphemeralHomeModuleBackend&) = delete;

  static std::unique_ptr<Config> GetConfig();

  // ModelProvider implementation.
  std::unique_ptr<ModelConfig> GetModelConfig() override;
  void ExecuteModelWithInput(const ModelProvider::Request& inputs,
                             ExecutionCallback callback) override;
};

}  // namespace segmentation_platform::home_modules

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_EPHEMERAL_HOME_MODULE_BACKEND_H_
