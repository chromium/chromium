// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_scoring_model_handler.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/omnibox/browser/autocomplete_scoring_model_executor.h"
#include "components/optimization_guide/core/model_handler.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using ModelInput = AutocompleteScoringModelExecutor::ModelInput;
using ModelOutput = AutocompleteScoringModelExecutor::ModelOutput;
using ::optimization_guide::proto::OptimizationTarget;

AutocompleteScoringModelHandler::AutocompleteScoringModelHandler(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    scoped_refptr<base::SequencedTaskRunner> model_executor_task_runner,
    OptimizationTarget optimization_target,
    const absl::optional<optimization_guide::proto::Any>& model_metadata)
    : optimization_guide::ModelHandler<ModelOutput, ModelInput>(
          model_provider,
          model_executor_task_runner,
          std::make_unique<AutocompleteScoringModelExecutor>(),
          /*model_inference_timeout=*/absl::nullopt,
          optimization_target,
          model_metadata) {
  // Keep the model in memory.
  SetShouldUnloadModelOnComplete(false);
}

AutocompleteScoringModelHandler::~AutocompleteScoringModelHandler() = default;
