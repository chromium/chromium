// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/edu_classifier_model_handler.h"

#include <optional>

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/edu_classifier_metadata.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/page_content_annotations/core/edu_classifier_model_executor.h"

namespace page_content_annotations {

namespace {

using ModelInput = EduClassifierModelExecutor::ModelInput;
using ModelOutput = EduClassifierModelExecutor::ModelOutput;

}  // namespace

EduClassifierModelHandler::EduClassifierModelHandler(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    scoped_refptr<base::SequencedTaskRunner> model_executor_task_runner)
    : CategoryClassifierModelHandler(
          model_provider,
          model_executor_task_runner,
          std::make_unique<EduClassifierModelExecutor>(),
          /*model_inference_timeout=*/std::nullopt,
          optimization_guide::proto::OPTIMIZATION_TARGET_EDU_CLASSIFIER,
          std::nullopt) {}

EduClassifierModelHandler::~EduClassifierModelHandler() = default;

std::optional<int64_t> EduClassifierModelHandler::GetRequiredEmbedderVersion()
    const {
  auto metadata = ParsedSupportedFeaturesForLoadedModel<
      optimization_guide::proto::EduClassifierMetadata>();
  if (metadata && metadata->has_required_embedder_version()) {
    return metadata->required_embedder_version();
  }
  return std::nullopt;
}

}  // namespace page_content_annotations
