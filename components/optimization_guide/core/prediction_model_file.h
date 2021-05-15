// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MODEL_FILE_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MODEL_FILE_H_

#include <memory>

#include "base/files/file_path.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace optimization_guide {

// Encapsulates information about a file containing a prediction model.
class PredictionModelFile {
 public:
  // Validates and creates a PredictionModelFile if valid.
  static std::unique_ptr<PredictionModelFile> Create(
      const proto::PredictionModel& model);
  ~PredictionModelFile();
  PredictionModelFile(const PredictionModelFile&) = delete;
  PredictionModelFile& operator=(const PredictionModelFile&) = delete;

  // Returns the version of the model file.
  int64_t GetVersion() const;

  // Returns the file path where the model file is stored.
  base::FilePath GetModelFilePath() const;

  // Returns the metadata that the server provided specific to this model, if
  // applicable.
  absl::optional<proto::Any> GetModelMetadata() const;

 private:
  PredictionModelFile(const base::FilePath& model_file_path,
                      const int64_t version,
                      const absl::optional<proto::Any>& model_metadata);
  base::FilePath model_file_path_;
  int64_t version_;
  absl::optional<proto::Any> model_metadata_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MODEL_FILE_H_
