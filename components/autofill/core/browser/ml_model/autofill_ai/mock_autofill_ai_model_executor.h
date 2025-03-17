// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_AI_MOCK_AUTOFILL_AI_MODEL_EXECUTOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_AI_MOCK_AUTOFILL_AI_MODEL_EXECUTOR_H_

#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_executor.h"
#include "components/autofill/core/common/form_data.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockAutofillAiModelExecutor : public AutofillAiModelExecutor {
 public:
  MockAutofillAiModelExecutor();
  MockAutofillAiModelExecutor(const MockAutofillAiModelExecutor&) = delete;
  MockAutofillAiModelExecutor& operator=(const MockAutofillAiModelExecutor&) =
      delete;
  ~MockAutofillAiModelExecutor() override;

  MOCK_METHOD(void, GetPredictions, (FormData), (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_AI_MOCK_AUTOFILL_AI_MODEL_EXECUTOR_H_
