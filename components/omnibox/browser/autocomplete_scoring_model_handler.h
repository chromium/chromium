// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_SCORING_MODEL_HANDLER_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_SCORING_MODEL_HANDLER_H_

#include <optional>

#include "base/gtest_prod_util.h"
#include "base/task/sequenced_task_runner.h"
#include "components/omnibox/browser/autocomplete_scoring_model_executor.h"
#include "components/optimization_guide/core/model_handler.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/autocomplete_scoring_model_metadata.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "third_party/metrics_proto/omnibox_scoring_signals.pb.h"

// Implements optimization_guide::ModelHandler for autocomplete scoring.
// Keeps scoring model in memory.
class AutocompleteScoringModelHandler
    : public optimization_guide::ModelHandler<
          AutocompleteScoringModelExecutor::ModelOutput,
          AutocompleteScoringModelExecutor::ModelInput> {
 public:
  using ScoringSignals = ::metrics::OmniboxScoringSignals;

  AutocompleteScoringModelHandler(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      scoped_refptr<base::SequencedTaskRunner> model_executor_task_runner,
      std::unique_ptr<AutocompleteScoringModelExecutor> model_executor,
      optimization_guide::proto::OptimizationTarget optimization_target,
      const std::optional<optimization_guide::proto::Any>& model_metadata);
  ~AutocompleteScoringModelHandler() override;

  // Disallow copy/assign.
  AutocompleteScoringModelHandler(const AutocompleteScoringModelHandler&) =
      delete;
  AutocompleteScoringModelHandler& operator=(
      const AutocompleteScoringModelHandler&) = delete;

  // Construct the model input from scoring signals. Signals are appended to the
  // input vector in the same order as signal specifications in the metadata.
  // Checks validness of signals and applies transformation if configured in
  // metadata. Returns nullopt if the model or metadata is missing.
  std::optional<std::vector<float>> GetModelInput(
      const ScoringSignals& scoring_signals);

  // Construct a batch model input from a vector of scoring signals.
  std::optional<std::vector<std::vector<float>>> GetBatchModelInput(
      const std::vector<const ScoringSignals*>& scoring_signals_vec);

 private:
  FRIEND_TEST_ALL_PREFIXES(AutocompleteScoringModelHandlerTest,
                           ExtractInputFromScoringSignalsTest);

  // Extracts the model input from scoring signals according to the model
  // metadata.
  std::vector<float> ExtractInputFromScoringSignals(
      const ScoringSignals& scoring_signals,
      const optimization_guide::proto::AutocompleteScoringModelMetadata&
          metadata);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_SCORING_MODEL_HANDLER_H_
