// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_SCORING_MODEL_SERVICE_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_SCORING_MODEL_SERVICE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/omnibox/browser/autocomplete_scoring_model_executor.h"
#include "components/omnibox/browser/autocomplete_scoring_model_handler.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// Autocomplete scoring service using machine learning models via
// OptimizationGuide's model handler.
class AutocompleteScoringModelService : public KeyedService {
 public:
  explicit AutocompleteScoringModelService(
      optimization_guide::OptimizationGuideModelProvider* model_provider);
  ~AutocompleteScoringModelService() override;

  // Disallow copy/assign.
  AutocompleteScoringModelService(const AutocompleteScoringModelService&) =
      delete;
  AutocompleteScoringModelService& operator=(
      const AutocompleteScoringModelService&) = delete;

  // Scores an autocomplete URL match with scoring signals.
  void ScoreAutocompleteUrlMatch(
      AutocompleteScoringModelExecutor::ModelInput input_signals,
      base::OnceCallback<void(
          const absl::optional<AutocompleteScoringModelExecutor::ModelOutput>&)>
          scoring_callback);

 private:
  scoped_refptr<base::SequencedTaskRunner> model_executor_task_runner_;

  // Autocomplete URL scoring model.
  std::unique_ptr<AutocompleteScoringModelHandler> url_scoring_model_handler_ =
      nullptr;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_SCORING_MODEL_SERVICE_H_
