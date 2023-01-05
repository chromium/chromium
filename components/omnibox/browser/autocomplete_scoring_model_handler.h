// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_SCORING_MODEL_HANDLER_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_SCORING_MODEL_HANDLER_H_

#include "base/task/sequenced_task_runner.h"
#include "components/omnibox/browser/autocomplete_scoring_model_executor.h"
#include "components/optimization_guide/core/model_handler.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// Implements optimization_guide::ModelHandler for autocomplete scoring.
// Keeps scoring model in memory.
class AutocompleteScoringModelHandler
    : public optimization_guide::ModelHandler<
          AutocompleteScoringModelExecutor::ModelOutput,
          AutocompleteScoringModelExecutor::ModelInput> {
 public:
  AutocompleteScoringModelHandler(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      scoped_refptr<base::SequencedTaskRunner> model_executor_task_runner,
      optimization_guide::proto::OptimizationTarget optimization_target,
      const absl::optional<optimization_guide::proto::Any>& model_metadata);
  ~AutocompleteScoringModelHandler() override;

  // Disallow copy/assign.
  AutocompleteScoringModelHandler(const AutocompleteScoringModelHandler&) =
      delete;
  AutocompleteScoringModelHandler& operator=(
      const AutocompleteScoringModelHandler&) = delete;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_SCORING_MODEL_HANDLER_H_
