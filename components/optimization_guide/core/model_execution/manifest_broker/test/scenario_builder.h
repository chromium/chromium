// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_TEST_SCENARIO_BUILDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_TEST_SCENARIO_BUILDER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ref.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/manifest_builder.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/test_manifest_asset_manager_component_state.h"
#include "components/optimization_guide/proto/manifest.pb.h"

namespace optimization_guide {

class ScenarioBuilder final {
 public:
  explicit ScenarioBuilder(
      TestManifestAssetManagerComponentState& component_state);
  ~ScenarioBuilder();

  ScenarioBuilder(const ScenarioBuilder&) = delete;
  ScenarioBuilder& operator=(const ScenarioBuilder&) = delete;

  ScenarioBuilder& AddBaseModel(const std::string& name);
  ScenarioBuilder& AddSafetyModel(const std::string& name);
  ScenarioBuilder& AddAdaptation(const std::string& name,
                                 const std::string& base_model);
  ScenarioBuilder& AddUnsafeSolution(const std::string& use_case,
                                     const std::string& model);
  ScenarioBuilder& AddSafeSolution(const std::string& use_case,
                                   const std::string& model,
                                   const std::string& safety_model);
  ScenarioBuilder& SetFeatureConfig(DeviceCategory category,
                                    const std::string& use_case,
                                    const proto::Any& config);

  void Finish();

  // Sets up a minimal scenario that should enable the "test" use case.
  static void MinimalTestScenario(
      TestManifestAssetManagerComponentState& component_state);

 private:
  raw_ref<TestManifestAssetManagerComponentState> state_;
  std::unique_ptr<ManifestComponentDirectory> manifest_directory_;
  ManifestBuilder builder;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_TEST_SCENARIO_BUILDER_H_
