// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_MANIFEST_SOLUTION_FACTORY_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_MANIFEST_SOLUTION_FACTORY_H_

#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "base/version.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest.h"
#include "components/optimization_guide/core/model_execution/model_broker_impl.h"
#include "components/optimization_guide/proto/manifest.pb.h"
#include "components/optimization_guide/proto/text_safety_model_metadata.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"
#include "services/on_device_model/public/cpp/service_client.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace optimization_guide {

class UsageTracker;

class ManifestSolutionFactory {
 public:
  enum class AssetUnavailableReason {
    kUninitialized,
    kNotDownloaded,
    kDownloading,
    kFailed,
  };
  struct AssetInfo {
    base::FilePath path;
  };
  using AssetState = base::expected<AssetInfo, AssetUnavailableReason>;

  struct BaseModelState {
    BaseModelState();
    ~BaseModelState();
    BaseModelState(const BaseModelState& other) = delete;
    BaseModelState& operator=(const BaseModelState& other) = delete;
    BaseModelState(BaseModelState&& other);
    BaseModelState& operator=(BaseModelState&& other);

    mojo::Remote<on_device_model::mojom::OnDeviceModel> remote_;
  };

  struct AdaptationState {
    AdaptationState();
    ~AdaptationState();
    AdaptationState(const AdaptationState& other) = delete;
    AdaptationState& operator=(const AdaptationState& other) = delete;
    AdaptationState(AdaptationState&& other);
    AdaptationState& operator=(AdaptationState&& other);

    mojo::Remote<on_device_model::mojom::OnDeviceModel> remote_;
  };

  struct SafetyModelState {
    SafetyModelState();
    ~SafetyModelState();
    SafetyModelState(const SafetyModelState& other) = delete;
    SafetyModelState& operator=(const SafetyModelState& other) = delete;
    SafetyModelState(SafetyModelState&& other);
    SafetyModelState& operator=(SafetyModelState&& other);

    mojo::Remote<on_device_model::mojom::TextSafetyModel> remote_;
  };

  struct SolutionState {
    SolutionState();
    ~SolutionState();
    SolutionState(const SolutionState& other) = delete;
    SolutionState& operator=(const SolutionState& other) = delete;
    SolutionState(SolutionState&& other);
    SolutionState& operator=(SolutionState&& other);

    enum Status {
      kNotLoaded,
      kLoading,
      kFailed,
      kReady,
    };
    Status status_ = kNotLoaded;
    proto::SolutionConfig config_;
  };

  ManifestSolutionFactory(Manifest manifest,
                          ModelBrokerImpl& broker_impl,
                          UsageTracker& usage_tracker,
                          on_device_model::ServiceClient& service_client,
                          base::OnceClosure on_init_complete);
  ~ManifestSolutionFactory();

  // Notifies the factory of a change in an asset's state.
  // This will may cause the factory to emit new Solutions.
  void UpdateAssetState(const std::string& asset_id, AssetState new_state);

  // Flush all use cases and emit new solutions for any that are now available.
  void UpdateSolutions();

  const Manifest& manifest() const { return manifest_; }

 private:
  class Solution;

  // Resolves a file reference to a file path.
  // Returns nullopt if the asset is not available.
  std::optional<base::FilePath> ResolveFile(const proto::FileReference& file);

  // Arrange for Solution configs stored in the given asset to be loaded.
  void LoadSolutionConfigsFrom(const std::string& asset_id,
                               base::OnceClosure on_complete);

  // Arrange for the SolutionState to be loaded from its config file.
  void LoadSolutionConfig(const std::string& solution_id,
                          const base::FilePath& config_path,
                          base::OnceClosure on_complete);

  // Creates a solution for the given use case, if possible.
  ModelBrokerImpl::MaybeSolution CreateSolutionForUseCase(
      const std::string& use_case_name);

  // Gets or loads the model.
  mojo::Remote<on_device_model::mojom::OnDeviceModel>& GetOrLoadModel(
      const std::string& model_id);

  void LoadBaseModel(const std::string& model_id, BaseModelState& state);
  void LoadAdaptation(const std::string& model_id, AdaptationState& state);

  mojo::Remote<on_device_model::mojom::TextSafetyModel>&
  GetOrLoadTextSafetyModel(const std::string& model_id);

  const raw_ref<ModelBrokerImpl> broker_impl_;
  const raw_ref<on_device_model::ServiceClient> service_client_;
  const raw_ref<UsageTracker> usage_tracker_;

  const Manifest manifest_;

  base::flat_map<std::string, AssetState> assets_;
  base::flat_map<std::string, BaseModelState> base_models_;
  base::flat_map<std::string, AdaptationState> adaptations_;
  base::flat_map<std::string, SafetyModelState> safety_models_;
  base::flat_map<std::string, SolutionState> solutions_;

  base::RepeatingClosure on_asset_init_;

  base::WeakPtrFactory<ManifestSolutionFactory> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_MANIFEST_SOLUTION_FACTORY_H_
