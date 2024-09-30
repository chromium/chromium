// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_FILLING_ENGINE_IMPL_H_
#define COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_FILLING_ENGINE_IMPL_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_filling_engine.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/user_annotations/user_annotations_types.h"

namespace autofill {
class FormData;
}  // namespace autofill

namespace optimization_guide::proto {
class AXTreeUpdate;
class FilledFormData;
}  // namespace optimization_guide::proto

namespace user_annotations {
class UserAnnotationsService;
}  // namespace user_annotations

namespace autofill_prediction_improvements {

class AutofillPredictionImprovementsFillingEngineImpl
    : public AutofillPredictionImprovementsFillingEngine {
 public:
  AutofillPredictionImprovementsFillingEngineImpl(
      optimization_guide::OptimizationGuideModelExecutor* model_executor,
      user_annotations::UserAnnotationsService* user_annotations_service);
  ~AutofillPredictionImprovementsFillingEngineImpl() override;

  // AutofillPredictionImprovementsFillingEngine:
  void GetPredictions(autofill::FormData form_data,
                      optimization_guide::proto::AXTreeUpdate ax_tree_update,
                      PredictionsReceivedCallback callback) override;

 private:
  // Invokes `callback` when user annotations were retrieved.
  void OnUserAnnotationsRetrieved(
      autofill::FormData form_data,
      optimization_guide::proto::AXTreeUpdate ax_tree_update,
      PredictionsReceivedCallback callback,
      user_annotations::UserAnnotationsEntries user_annotations);

  // Invokes `callback` when model execution response has been returned.
  void OnModelExecuted(
      autofill::FormData form_data,
      PredictionsReceivedCallback callback,
      optimization_guide::OptimizationGuideModelExecutionResult
          execution_result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  static PredictionsByGlobalId ExtractPredictions(
      const autofill::FormData& form_data,
      const optimization_guide::proto::FilledFormData& form_data_proto);

  raw_ptr<optimization_guide::OptimizationGuideModelExecutor> model_executor_ =
      nullptr;
  raw_ptr<user_annotations::UserAnnotationsService> user_annotations_service_ =
      nullptr;

  base::WeakPtrFactory<AutofillPredictionImprovementsFillingEngineImpl>
      weak_ptr_factory_{this};
};

}  // namespace autofill_prediction_improvements

#endif  // COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_FILLING_ENGINE_IMPL_H_
