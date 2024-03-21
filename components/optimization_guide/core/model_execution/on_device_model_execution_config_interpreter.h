// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_EXECUTION_CONFIG_INTERPRETER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_EXECUTION_CONFIG_INTERPRETER_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"
#include "components/optimization_guide/core/model_execution/substitution.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"

namespace optimization_guide {

class OnDeviceModelExecutionConfigInterpreter {
 public:
  OnDeviceModelExecutionConfigInterpreter();
  ~OnDeviceModelExecutionConfigInterpreter();

  // Updates the config that `this` operates on with the config contained in
  // `file_dir`.
  void UpdateConfigWithFileDir(const base::FilePath& file_dir);

  // Clears the current state of `this` that may be associated with a previous
  // config.
  void ClearState();

  // Returns a "copy" of the current adapter for a particular feature.
  scoped_refptr<const OnDeviceModelFeatureAdapter> GetAdapter(
      proto::ModelExecutionFeature feature) const;

 private:
  // Populates `adapters_` based on `config`.
  void PopulateFeatureConfigs(
      std::unique_ptr<proto::OnDeviceModelExecutionConfig> config);

  // The task runner to process new config files on.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // Map from feature to associated state.
  base::flat_map<proto::ModelExecutionFeature,
                 scoped_refptr<OnDeviceModelFeatureAdapter>>
      adapters_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<OnDeviceModelExecutionConfigInterpreter>
      weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_EXECUTION_CONFIG_INTERPRETER_H_
