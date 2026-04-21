// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_TEST_MANIFEST_BUILDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_TEST_MANIFEST_BUILDER_H_

#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest.h"
#include "components/optimization_guide/proto/manifest.pb.h"

namespace optimization_guide {

inline constexpr std::string kNoSafetyModel = "";

// Well-known use case names.
inline constexpr char kLanguageModelUseCase[] = "language_model";
inline constexpr char kProofreaderUseCase[] = "proofreader";
inline constexpr char kSummarizerUseCase[] = "summarizer";
inline constexpr char kWriterUseCase[] = "writer";

proto::OnDemandComponent OnDemandComponent(const std::string& public_key,
                                           const std::string& target_version);

proto::FileReference FileReference(const std::string& asset_id,
                                   const std::string& relative_path);

inline proto::FileReference ManifestFileReference(
    const std::string& relative_path) {
  return FileReference(kManifestAssetName, relative_path);
}

struct BaseModelRecipeArgs {
  BaseModelRecipeArgs(proto::BaseModelRecipe::BackendType backend_type,
                      proto::BaseModelRecipe::PerformanceHint performance_hint,
                      std::vector<int32_t> supported_ranks,
                      int32_t max_tokens);
  ~BaseModelRecipeArgs();
  BaseModelRecipeArgs(const BaseModelRecipeArgs&);
  BaseModelRecipeArgs& operator=(const BaseModelRecipeArgs&);

  proto::BaseModelRecipe::BackendType backend_type;
  proto::BaseModelRecipe::PerformanceHint performance_hint;
  std::vector<int32_t> supported_ranks;
  int32_t max_tokens;
};

proto::BaseModelRecipe BaseModelRecipe(proto::FileReference weights_file,
                                       BaseModelRecipeArgs args);

proto::AdaptationRecipe AdaptationRecipe(const std::string& base_model_id,
                                         proto::FileReference weights_file);

proto::SafetyModelRecipe SafetyModelRecipe(
    proto::FileReference weights_file,
    proto::FileReference language_detection_model_file);

proto::SolutionRecipe SolutionRecipe(const std::string& model_recipe_id,
                                     const std::string& safety_model_recipe_id,
                                     proto::FileReference config_file);

// Declares a 'use_case' on 'device'.
struct DeviceUseCase {
  DeviceCategory device;
  std::string use_case;
};

// Builder for a Manifest proto.
class ManifestBuilder {
 public:
  ManifestBuilder();
  ~ManifestBuilder();

  ManifestBuilder& Add(const std::string& name,
                       proto::OnDemandComponent component);

  ManifestBuilder& Add(const std::string& name, proto::BaseModelRecipe recipe);

  ManifestBuilder& Add(const std::string& name, proto::AdaptationRecipe recipe);

  ManifestBuilder& Add(const std::string& name,
                       proto::SafetyModelRecipe recipe);

  ManifestBuilder& Add(const std::string& name, proto::SolutionRecipe recipe);

  ManifestBuilder& Add(const DeviceUseCase& device_use_case,
                       const std::string& solution_recipe_id);

  // Declares a recipe and the use cases it serves.
  ManifestBuilder& Add(const std::string& name,
                       proto::SolutionRecipe recipe,
                       const std::vector<DeviceUseCase>& device_use_cases);

  // Declares a device-specific use case.
  ManifestBuilder& Add(const DeviceUseCase& use_case,
                       proto::SolutionRecipe recipe);

  ManifestBuilder& SetFeatureConfig(DeviceCategory device,
                                    const std::string& name,
                                    proto::Any config);

  proto::Manifest Build();

 private:
  proto::Manifest manifest_;
};

// Constructs a Manifest component directory.
class ManifestComponentDirectory {
 public:
  explicit ManifestComponentDirectory(const proto::Manifest& manifest);
  ~ManifestComponentDirectory();

  // Replaces the manifest in the directory.
  ManifestComponentDirectory& Add(const proto::Manifest& manifest);
  // Adds a new solution config to the directory, overwriting existing ones.
  ManifestComponentDirectory& Add(const std::string& filename,
                                  const proto::SolutionConfig& config);

  base::FilePath path() const { return temp_dir_.GetPath(); }

 private:
  base::ScopedTempDir temp_dir_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_TEST_MANIFEST_BUILDER_H_
