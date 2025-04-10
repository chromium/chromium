// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_AI_AUTOFILL_AI_MODEL_EXECUTOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_AI_AUTOFILL_AI_MODEL_EXECUTOR_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/keyed_service/core/keyed_service.h"

namespace optimization_guide::proto {
class AnnotatedPageContent;
}

namespace autofill {

class FormData;

// Responsible for managing calls to the AutofillAI server model via
// optimization guide infrastructure.
class AutofillAiModelExecutor : public KeyedService {
 public:
  // Retrieves predictions for `form_data` and writes them into the cache once
  // the model execution completes. Errors during model execution also lead to
  // cache writes, but with empty values. If there is already an ongoing cache
  // request for a form of the same signature, the model is not run.
  // If `annotated_page_content` is set, it includes it in its upload to the
  // model.
  virtual void GetPredictions(
      FormData form_data,
      base::OnceCallback<void(const FormGlobalId&)> on_model_executed,
      std::optional<optimization_guide::proto::AnnotatedPageContent>
          annotated_page_content) = 0;

  virtual base::WeakPtr<AutofillAiModelExecutor> GetWeakPtr() = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_AI_AUTOFILL_AI_MODEL_EXECUTOR_H_
