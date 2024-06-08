// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_METADATA_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_METADATA_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"
#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"
#include "components/optimization_guide/core/model_execution/substitution.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"

namespace optimization_guide {

class OnDeviceModelMetadata final {
 public:
  ~OnDeviceModelMetadata();

  // Bindable constructor for an OnDeviceModelMetadata.
  static std::unique_ptr<OnDeviceModelMetadata> New(
      base::FilePath model_path,
      std::string version,
      const OnDeviceBaseModelSpec& model_spec,
      std::unique_ptr<proto::OnDeviceModelExecutionConfig> config);

  // Returns a "copy" of the current adapter for a particular feature.
  scoped_refptr<const OnDeviceModelFeatureAdapter> GetAdapter(
      proto::ModelExecutionFeature feature) const;

  const base::FilePath& model_path() const { return model_path_; }
  const std::string& version() const { return version_; }
  const OnDeviceBaseModelSpec& model_spec() const { return model_spec_; }
  const proto::OnDeviceModelValidationConfig& validation_config() const {
    return validation_config_;
  }

 private:
  OnDeviceModelMetadata(
      const base::FilePath& model_path,
      const std::string& version,
      const OnDeviceBaseModelSpec& model_spec,
      std::unique_ptr<proto::OnDeviceModelExecutionConfig> config);

  base::FilePath model_path_;
  std::string version_;
  OnDeviceBaseModelSpec model_spec_;
  proto::OnDeviceModelValidationConfig validation_config_;

  // Map from feature to associated state.
  base::flat_map<proto::ModelExecutionFeature,
                 scoped_refptr<OnDeviceModelFeatureAdapter>>
      adapters_;
};

// Provides a stream of updated ModelMetadatas from component states.
// Provides null values between valid states.
class OnDeviceModelMetadataLoader final
    : public OnDeviceModelComponentStateManager::Observer {
 public:
  using OnLoadFn =
      base::RepeatingCallback<void(std::unique_ptr<OnDeviceModelMetadata>)>;

  OnDeviceModelMetadataLoader(OnLoadFn on_load_fn,
                              base::WeakPtr<OnDeviceModelComponentStateManager>
                                  on_device_component_state_manager);
  ~OnDeviceModelMetadataLoader() final;

  // OnDeviceModelComponentStateManager::Observer.
  void StateChanged(const OnDeviceModelComponentState* state) final;

  // Loads OnDeviceModelMetadata with the data from file_dir.
  void Load(const base::FilePath& model_path,
            const std::string& version,
            const OnDeviceBaseModelSpec& model_spec);

 private:
  // Provides a null ModelMetadata in the stream.
  void Invalidate();

  OnLoadFn on_load_fn_;

  // The manager we observe updates from.
  base::WeakPtr<OnDeviceModelComponentStateManager>
      on_device_component_state_manager_;

  // The task runner to process new config files on.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_METADATA_H_
