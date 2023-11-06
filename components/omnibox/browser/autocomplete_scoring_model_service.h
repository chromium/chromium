// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_SCORING_MODEL_SERVICE_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_SCORING_MODEL_SERVICE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/omnibox/browser/autocomplete_scoring_model_executor.h"
#include "components/omnibox/browser/autocomplete_scoring_model_handler.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

// Autocomplete scoring service using machine learning models via
// OptimizationGuide's model handler.
class AutocompleteScoringModelService : public KeyedService {
 public:
  using Result = std::tuple<absl::optional<float>, std::string>;
  using ResultCallback = base::OnceCallback<void(Result)>;
  using BatchResult = std::vector<Result>;
  using BatchResultCallback = base::OnceCallback<void(BatchResult)>;
  using ModelOutput = AutocompleteScoringModelExecutor::ModelOutput;
  using ScoringSignals =
      ::metrics::OmniboxEventProto::Suggestion::ScoringSignals;

  explicit AutocompleteScoringModelService(
      optimization_guide::OptimizationGuideModelProvider* model_provider);
  ~AutocompleteScoringModelService() override;

  // Disallow copy/assign.
  AutocompleteScoringModelService(const AutocompleteScoringModelService&) =
      delete;
  AutocompleteScoringModelService& operator=(
      const AutocompleteScoringModelService&) = delete;

  // Passthrough to
  // `AutocompleteScoringModelHandler::AddOnModelUpdatedCallback()`.
  void AddOnModelUpdatedCallback(base::OnceClosure callback);

  // Returns the version from the model info.
  int GetModelVersion() const;

  // Invokes the model to score the given `scoring_signals` and calls
  // `result_callback` with an optional prediction score from the model. If the
  // model is not available or the model input cannot be generated from the
  // signals, calls `result_callback` with an empty prediction score.
  virtual void ScoreAutocompleteUrlMatch(
      base::CancelableTaskTracker* tracker,
      const ScoringSignals& scoring_signals,
      const std::string& stripped_destination_url,
      ResultCallback result_callback);

  // Invokes the model to score the given batch of `scoring_signals` and calls
  // `batch_result_callback` with a batch of optional prediction scores from the
  // model. If the model is not available or any of the model inputs cannot be
  // generated from the signals, calls `result_callback` with an empty vector of
  // prediction scores.
  virtual void BatchScoreAutocompleteUrlMatches(
      base::CancelableTaskTracker* tracker,
      const std::vector<const ScoringSignals*>& batch_scoring_signals,
      const std::vector<std::string>& stripped_destination_urls,
      BatchResultCallback batch_result_callback);

  // Synchronous batch scoring. Returns a vector of batch results.
  // Returns an empty vector if model is not available or the input signals are
  // invalid.
  virtual std::vector<Result> BatchScoreAutocompleteUrlMatchesSync(
      const std::vector<const ScoringSignals*>& batch_scoring_signals,
      const std::vector<std::string>& stripped_destination_urls);

 private:
  // Returns whether the scoring model is enabled and loaded.
  bool UrlScoringModelAvailable();

  // Processes the model output and invokes the callback with the relevance
  // score from the model output. Invokes the callback with nullopt if the model
  // output is nullopt or an empty vector (which is unexpected).
  void ProcessModelOutput(ResultCallback result_callback,
                          const std::string& stripped_destination_url,
                          const absl::optional<ModelOutput>& model_output);

  void ProcessBatchModelOutput(
      BatchResultCallback batch_result_callback,
      const std::vector<std::string>& stripped_destination_urls,
      const std::vector<absl::optional<ModelOutput>>& batch_model_output);

  // Extracts model output values and associates them with the stripped
  // destination urls.
  std::vector<Result> GetBatchResultFromModelOutput(
      const std::vector<std::string>& stripped_destination_urls,
      const std::vector<absl::optional<ModelOutput>>& batch_model_output);

  scoped_refptr<base::SequencedTaskRunner> model_executor_task_runner_;

  // Autocomplete URL scoring model.
  std::unique_ptr<AutocompleteScoringModelHandler> url_scoring_model_handler_;

  base::WeakPtrFactory<AutocompleteScoringModelService> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_SCORING_MODEL_SERVICE_H_
