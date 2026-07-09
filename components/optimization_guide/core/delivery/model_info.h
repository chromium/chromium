// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_MODEL_INFO_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_MODEL_INFO_H_

#include <cstdint>
#include <memory>
#include <optional>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace optimization_guide {

// Encapsulates information about a prediction model like its file path on disk
// and other metadata.
//
// Note: TestModelInfoBuilder can be used to facilitate creation of ModelInfo
// for testing.
struct ModelInfo {
  // Validates and creates a ModelInfo if valid.
  static std::unique_ptr<ModelInfo> Create(const proto::PredictionModel& model);

  // Returns the absolute file path of any additional files that were packaged
  // along with the model based on `base_name`.
  std::optional<base::FilePath> GetAdditionalFileWithBaseName(
      const base::FilePath::StringType& base_name) const;

  base::FilePath model_file_path;
  base::flat_set<base::FilePath> additional_files;
  int64_t version;
  std::optional<proto::Any> model_metadata;
};

// Loads the model and verifies if the model files exist and returns the
// model. Otherwise nullptr is returned on any failures.
// Must be called on a background thread that allows blocking file I/O.
std::unique_ptr<proto::PredictionModel> LoadAndVerifyModelOffThread(
    proto::OptimizationTarget optimization_target,
    const base::FilePath& base_model_dir);

// Loads the model, verifies if the model files exist, and returns the
// ModelInfo. Otherwise nullptr is returned on any failures.
// Must be called on a background thread that allows blocking file I/O.
std::unique_ptr<ModelInfo> LoadAndVerifyModelInfoOffThread(
    proto::OptimizationTarget optimization_target,
    const base::FilePath& base_model_dir);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_MODEL_INFO_H_
