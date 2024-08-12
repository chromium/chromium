// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_FILLING_ENGINE_H_
#define COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_FILLING_ENGINE_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"

namespace autofill {
class FormData;
}  // namespace autofill

namespace optimization_guide::proto {
class AXTreeUpdate;
class FilledFormData;
class UserAnnotationsEntry;
}  // namespace optimization_guide::proto

namespace user_annotations {
class UserAnnotationsService;
}  // namespace user_annotations

namespace autofill_prediction_improvements {

// The filling engine that provides autofill predictions improvements.
class AutofillPredictionImprovementsFillingEngine {
 public:
  AutofillPredictionImprovementsFillingEngine(
      optimization_guide::OptimizationGuideModelExecutor* model_executor,
      user_annotations::UserAnnotationsService* user_annotations_service);
  ~AutofillPredictionImprovementsFillingEngine();

  // Get predictions for `form_data` with context of `ax_tree_update`. Invokes
  // callback` when done.
  void GetPredictions(
      autofill::FormData form_data,
      optimization_guide::proto::AXTreeUpdate ax_tree_update,
      base::OnceCallback<void(base::expected<autofill::FormData, bool>)>
          callback);

 private:
  // Callback invoked when user annotations were retrieved.
  void OnUserAnnotationsRetrieved(
      autofill::FormData form_data,
      optimization_guide::proto::AXTreeUpdate ax_tree_update,
      base::OnceCallback<void(base::expected<autofill::FormData, bool>)>
          callback,
      std::vector<optimization_guide::proto::UserAnnotationsEntry>
          user_annotations);

  // Callback invoked when model execution response has been returned.
  void OnModelExecuted(
      autofill::FormData form_data,
      base::OnceCallback<void(base::expected<autofill::FormData, bool>)>
          callback,
      optimization_guide::OptimizationGuideModelExecutionResult
          execution_result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  static void FillFormDataWithResponse(
      autofill::FormData& form_data,
      const optimization_guide::proto::FilledFormData& form_data_proto);

  raw_ptr<optimization_guide::OptimizationGuideModelExecutor> model_executor_;
  raw_ptr<user_annotations::UserAnnotationsService> user_annotations_service_;

  base::WeakPtrFactory<AutofillPredictionImprovementsFillingEngine>
      weak_ptr_factory_{this};
};

}  // namespace autofill_prediction_improvements

#endif  // COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_FILLING_ENGINE_H_
