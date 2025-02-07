// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_MANAGER_TEST_API_H_
#define COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_MANAGER_TEST_API_H_

#include "base/check_deref.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/autofill_ai/core/browser/autofill_ai_manager.h"

namespace autofill_ai {

class AutofillAiLogger;

class AutofillAiManagerTestApi {
 public:
  explicit AutofillAiManagerTestApi(AutofillAiManager* manager)
      : manager_(CHECK_DEREF(manager)) {}

  AutofillAiLogger& logger() { return manager_->logger_; }

 private:
  raw_ref<AutofillAiManager> manager_;
};

inline AutofillAiManagerTestApi test_api(AutofillAiManager& manager) {
  return AutofillAiManagerTestApi(&manager);
}

}  // namespace autofill_ai

#endif  // COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_MANAGER_TEST_API_H_
