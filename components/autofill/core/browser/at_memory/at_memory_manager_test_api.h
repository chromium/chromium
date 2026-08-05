// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_MANAGER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_MANAGER_TEST_API_H_

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/at_memory/at_memory_manager.h"
#include "components/autofill/core/browser/at_memory/at_memory_metrics_recorder.h"

namespace autofill {

class AtMemoryManagerTestApi {
 public:
  explicit AtMemoryManagerTestApi(AtMemoryManager* manager)
      : manager_(CHECK_DEREF(manager)) {}

  AtMemoryMetricsRecorder* at_memory_metrics_recorder() {
    return manager_->session_state_
               ? manager_->session_state_->metrics_recorder.get()
               : nullptr;
  }

 private:
  raw_ref<AtMemoryManager> manager_;
};

inline AtMemoryManagerTestApi test_api(AtMemoryManager& manager) {
  return AtMemoryManagerTestApi(&manager);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_MANAGER_TEST_API_H_
