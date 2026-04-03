// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/category_classifier_model_handler.h"

#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/category_classifier_metadata.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/page_content_annotations/core/category_classifier_model_executor.h"

namespace page_content_annotations {

CategoryClassifierModelHandler::CategoryClassifierModelHandler(
    optimization_guide::proto::OptimizationTarget optimization_target,
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    scoped_refptr<base::SequencedTaskRunner> model_executor_task_runner)
    : CategoryClassifierModelHandler(
          model_provider,
          model_executor_task_runner,
          std::make_unique<CategoryClassifierModelExecutor>(),
          /*model_inference_timeout=*/std::nullopt,
          optimization_target,
          std::nullopt) {}

CategoryClassifierModelHandler::~CategoryClassifierModelHandler() = default;

std::optional<int64_t>
CategoryClassifierModelHandler::GetRequiredEmbedderVersion() const {
  auto metadata = ParsedSupportedFeaturesForLoadedModel<
      optimization_guide::proto::CategoryClassifierMetadata>();
  if (metadata && metadata->has_required_embedder_version()) {
    return metadata->required_embedder_version();
  }
  return std::nullopt;
}

}  // namespace page_content_annotations
