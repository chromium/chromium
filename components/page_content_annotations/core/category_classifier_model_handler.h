// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_CATEGORY_CLASSIFIER_MODEL_HANDLER_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_CATEGORY_CLASSIFIER_MODEL_HANDLER_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "components/optimization_guide/core/inference/model_handler.h"

namespace page_content_annotations {

// An interface for category classifier model handlers that provides the
// required embedder version.
class CategoryClassifierModelHandler
    : public optimization_guide::ModelHandler<float,
                                              const std::vector<float>&> {
 public:
  using optimization_guide::ModelHandler<float, const std::vector<float>&>::
      ModelHandler;

  // Returns the version of the embedder model that this classifier model was
  // trained on. Returns std::nullopt if the model or metadata is not available.
  virtual std::optional<int64_t> GetRequiredEmbedderVersion() const = 0;
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_CATEGORY_CLASSIFIER_MODEL_HANDLER_H_
