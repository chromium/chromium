// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_TEST_MODEL_INFO_BUILDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_TEST_MODEL_INFO_BUILDER_H_

#include <memory>
#include <optional>

#include "base/containers/flat_set.h"
#include "components/optimization_guide/core/delivery/model_info.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace optimization_guide {

// A builder for ModelInfo's that should only be used in testing.
// In production code, these classes are assembled by internal OptGuide code but
// external callers may wish to create their own for testing. An unmodified
// builder will return a valid ModelInfo when built with no
// metadata and dummy values for the model file path (which won't exist) and
// version.
class TestModelInfoBuilder {
 public:
  TestModelInfoBuilder();
  explicit TestModelInfoBuilder(const ModelInfo& model_info);
  ~TestModelInfoBuilder();

  TestModelInfoBuilder& SetModelFilePath(const base::FilePath& file_path);

  TestModelInfoBuilder& SetAdditionalFiles(
      const base::flat_set<base::FilePath>& additional_files);

  TestModelInfoBuilder& RemoveAdditionalFileWithBasename(
      const base::FilePath::StringType& base_name);

  TestModelInfoBuilder& SetVersion(int64_t version);

  TestModelInfoBuilder& SetModelMetadata(
      std::optional<proto::Any> model_metadata);

  std::unique_ptr<ModelInfo> Build();

 private:
  proto::PredictionModel model_;
};

// File paths that can be used in testing, handling platform differences, namely
// C:\ in Windows.
extern const char kTestAbsoluteFilePath[];

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_TEST_MODEL_INFO_BUILDER_H_
