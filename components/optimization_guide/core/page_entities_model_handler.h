// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_ENTITIES_MODEL_HANDLER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_ENTITIES_MODEL_HANDLER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "components/optimization_guide/core/entity_metadata.h"
#include "components/optimization_guide/core/model_info.h"
#include "components/optimization_guide/core/page_content_annotation_job_executor.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/category.h"

namespace optimization_guide {

// The PageEntitiesModelHandler is responsible for executing the PAGE_ENTITIES
// model.
class PageEntitiesModelHandler : public PageContentAnnotationJobExecutor {
 public:
  virtual ~PageEntitiesModelHandler() = default;

  using PageEntitiesMetadataModelExecutedCallback = base::OnceCallback<void(
      const absl::optional<std::vector<ScoredEntityMetadata>>&)>;

  // Annotates |text| with page entities likely represented on the page,
  // returning the entity metadata in the reader's locale with associated score.
  // Invokes |callback| when done.
  virtual void ExecuteModelWithInput(
      const std::string& text,
      PageEntitiesMetadataModelExecutedCallback callback) = 0;

  using PageEntitiesModelEntityMetadataRetrievedCallback =
      base::OnceCallback<void(const absl::optional<EntityMetadata>&)>;

  // Retrieves the metadata associated with |entity_id|. Invokes |callback|
  // when done.
  virtual void GetMetadataForEntityId(
      const std::string& entity_id,
      PageEntitiesModelEntityMetadataRetrievedCallback callback) = 0;

  // Runs |callback| now if a model is loaded or the next time |OnModelUpdated|
  // is called.
  virtual void AddOnModelUpdatedCallback(base::OnceClosure callback) = 0;

  // Returns the ModelInfo for a currently loaded model, if available.
  virtual absl::optional<ModelInfo> GetModelInfo() const = 0;

  // PageContentAnnotationJobExecutor:
  void ExecuteOnSingleInput(
      AnnotationType annotation_type,
      const std::string& input,
      base::OnceCallback<void(const BatchAnnotationResult&)> callback) override;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_ENTITIES_MODEL_HANDLER_H_
