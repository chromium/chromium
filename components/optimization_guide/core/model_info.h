// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_INFO_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_INFO_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace optimization_guide {

// Encapsulates information about a prediction model like its file path on disk
// and other metadata.
//
// Testing: This class is created by OptGuide code in production and isn't meant
// to be created by external consumers except for testing. For that purpose, use
// |TestModelInfoBuilder|.
class ModelInfo {
 public:
  // Validates and creates a ModelInfo if valid.
  static std::unique_ptr<ModelInfo> Create(const proto::PredictionModel& model);
  ~ModelInfo();
  ModelInfo(const ModelInfo&);

  // Returns the version of the model file.
  int64_t GetVersion() const;

  // Returns the absolute file path where the model file is stored. This is the
  // file that should be loaded into the TFLite Interpreter.
  base::FilePath GetModelFilePath() const;

  // Returns a set of absolute file paths of any additional files that were
  // packaged along with the model.
  base::flat_set<base::FilePath> GetAdditionalFiles() const;

  // Returns the metadata that the server provided specific to this model, if
  // applicable.
  absl::optional<proto::Any> GetModelMetadata() const;

 private:
  ModelInfo(const base::FilePath& model_file_path,
            const base::flat_set<base::FilePath>& additional_files,
            const int64_t version,
            const absl::optional<proto::Any>& model_metadata);
  base::FilePath model_file_path_;
  base::flat_set<base::FilePath> additional_files_;
  int64_t version_;
  absl::optional<proto::Any> model_metadata_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_INFO_H_
