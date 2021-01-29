// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/prediction_model_file.h"

#include "base/memory/ptr_util.h"
#include "components/optimization_guide/core/optimization_guide_util.h"

namespace optimization_guide {

PredictionModelFile::PredictionModelFile(
    const base::FilePath& model_file_path,
    const int64_t version,
    const base::Optional<proto::Any>& model_metadata)
    : model_file_path_(model_file_path),
      version_(version),
      model_metadata_(model_metadata) {}

PredictionModelFile::~PredictionModelFile() = default;

// static
std::unique_ptr<PredictionModelFile> PredictionModelFile::Create(
    const proto::PredictionModel& model) {
  base::Optional<base::FilePath> model_file_path =
      GetFilePathFromPredictionModel(model);
  if (!model_file_path)
    return nullptr;
  if (!model.model_info().has_version())
    return nullptr;

  base::Optional<proto::Any> model_metadata;
  if (model.model_info().has_model_metadata())
    model_metadata = model.model_info().model_metadata();

  // Private ctor, so we can't use std::make_unique.
  return base::WrapUnique(new PredictionModelFile(
      *model_file_path, model.model_info().version(), model_metadata));
}

base::FilePath PredictionModelFile::GetModelFilePath() const {
  return model_file_path_;
}

int64_t PredictionModelFile::GetVersion() const {
  return version_;
}

base::Optional<proto::Any> PredictionModelFile::GetModelMetadata() const {
  return model_metadata_;
}

}  // namespace optimization_guide
