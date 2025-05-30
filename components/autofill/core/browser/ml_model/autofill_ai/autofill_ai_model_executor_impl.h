// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_AI_AUTOFILL_AI_MODEL_EXECUTOR_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_AI_AUTOFILL_AI_MODEL_EXECUTOR_IMPL_H_

#include <memory>
#include <optional>
#include <string_view>

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_executor.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/optimization_guide/core/model_quality/model_quality_logs_uploader_service.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/features/forms_classifications.pb.h"
#include "components/optimization_guide/proto/features/model_prototyping.pb.h"

namespace autofill {

class AutofillAiModelCache;
class FormData;

// Enum describing whether an AutofillAI model execution was successful.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AutofillAiModelExecutionStatus {
  // One or more field indices returned by the model are out of bounds or
  // duplicates.
  kErrorInvalidFieldIndex = 0,
  // The server returned an error or timed out.
  kErrorServerError = 1,
  // The server replied with the wrong response proto type.
  kErrorWrongResponseType = 2,
  // The model returned a result, but it was empty.
  kSuccessEmptyResult = 3,
  // The model returned a valid, non-empty result.
  kSuccessNonEmptyResult = 4,
  kMaxValue = kSuccessNonEmptyResult
};

inline constexpr std::string_view kUmaAutofillAiModelExecutionStatus =
    "Autofill.Ai.ModelExecutionStatus";

class AutofillAiModelExecutorImpl : public AutofillAiModelExecutor {
 public:
  AutofillAiModelExecutorImpl(
      AutofillAiModelCache* model_cache,
      optimization_guide::OptimizationGuideModelExecutor* model_executor,
      optimization_guide::ModelQualityLogsUploaderService* mqls_uploader);
  ~AutofillAiModelExecutorImpl() override;

  // AutofillAiModelExecutor:
  void GetPredictions(
      FormData form_data,
      base::OnceCallback<void(const FormGlobalId&)> on_model_executed,
      std::optional<optimization_guide::proto::AnnotatedPageContent>
          annotated_page_content) override;
  base::WeakPtr<AutofillAiModelExecutor> GetWeakPtr() override;

 private:
  // Writes the model execution response into the cache.
  void OnModelExecuted(
      FormData form_data,
      optimization_guide::OptimizationGuideModelExecutionResult
          execution_result,
      std::unique_ptr<
          optimization_guide::proto::FormsClassificationsLoggingData>
          logging_data);

  // Uploads a stripped request and the response of a model run to MQLS.
  void LogModelPredictions(
      std::unique_ptr<
          optimization_guide::proto::FormsClassificationsLoggingData>
          logging_data);

  // The cache into which the model responses are written.
  const raw_ref<AutofillAiModelCache> model_cache_;

  const raw_ref<optimization_guide::OptimizationGuideModelExecutor>
      model_executor_;
  const raw_ptr<optimization_guide::ModelQualityLogsUploaderService>
      mqls_uploader_;

  // Form signatures for which a query is currently ongoing. The goal is to
  // avoid multiple queries for the same form at the same time.
  base::flat_set<FormSignature> ongoing_queries_;

  base::WeakPtrFactory<AutofillAiModelExecutorImpl> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_AI_AUTOFILL_AI_MODEL_EXECUTOR_IMPL_H_
