// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_TEST_UTILS_H_

#include <ostream>

#include "components/autofill_ai/core/browser/suggestion/autofill_ai_model_executor.h"

namespace autofill_ai {

// For GTest.
void PrintTo(const AutofillAiModelExecutor::Prediction& prediction,
             std::ostream* os);

}  // namespace autofill_ai

#endif  // COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_TEST_UTILS_H_
