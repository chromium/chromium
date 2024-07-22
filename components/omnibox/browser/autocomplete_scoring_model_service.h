// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_SCORING_MODEL_SERVICE_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_SCORING_MODEL_SERVICE_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/lru_cache.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/omnibox/browser/autocomplete_scoring_model_executor.h"
#include "components/omnibox/browser/autocomplete_scoring_model_handler.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "third_party/metrics_proto/omnibox_scoring_signals.pb.h"

// Autocomplete scoring service using machine learning models via
// OptimizationGuide's model handler.
class AutocompleteScoringModelService : public KeyedService {
 public:
  using Result = std::optional<float>;
  using ModelOutput = AutocompleteScoringModelExecutor::ModelOutput;
  using ScoringSignals = ::metrics::OmniboxScoringSignals;

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

  // Synchronous batch scoring. Returns a vector of batch results.
  // Returns an empty vector if model is not available or the input signals are
  // invalid.
  virtual std::vector<Result> BatchScoreAutocompleteUrlMatchesSync(
      const std::vector<const ScoringSignals*>& batch_scoring_signals);

 private:
  // Returns whether the scoring model is enabled and loaded.
  bool UrlScoringModelAvailable();

  scoped_refptr<base::SequencedTaskRunner> model_executor_task_runner_;

  // Autocomplete URL scoring model.
  std::unique_ptr<AutocompleteScoringModelHandler> url_scoring_model_handler_;

  // Cache mapping each ML model input vector to the corresponding ML model
  // output score.
  base::LRUCache<std::vector<float>, float> score_cache_;

  base::WeakPtrFactory<AutocompleteScoringModelService> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_SCORING_MODEL_SERVICE_H_
