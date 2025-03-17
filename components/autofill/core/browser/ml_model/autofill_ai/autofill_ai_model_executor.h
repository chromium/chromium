// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_AI_AUTOFILL_AI_MODEL_EXECUTOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_AI_AUTOFILL_AI_MODEL_EXECUTOR_H_

#include "components/keyed_service/core/keyed_service.h"

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
  virtual void GetPredictions(FormData form_data) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_AI_AUTOFILL_AI_MODEL_EXECUTOR_H_
