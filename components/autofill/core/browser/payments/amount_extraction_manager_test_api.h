// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AMOUNT_EXTRACTION_MANAGER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AMOUNT_EXTRACTION_MANAGER_TEST_API_H_

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/payments/amount_extraction_manager.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"

namespace autofill::payments {

class AmountExtractionManagerTestApi {
 public:
  explicit AmountExtractionManagerTestApi(
      AmountExtractionManager* amount_extraction_manager)
      : amount_extraction_manager_(CHECK_DEREF(amount_extraction_manager)) {}
  AmountExtractionManagerTestApi(const AmountExtractionManagerTestApi&) =
      delete;
  AmountExtractionManagerTestApi& operator=(
      const AmountExtractionManagerTestApi&) = delete;
  ~AmountExtractionManagerTestApi() = default;

  AutofillDriver* GetMainFrameDriver() {
    return amount_extraction_manager_->GetMainFrameDriver();
  }

  bool GetSearchRequestPending() {
    return amount_extraction_manager_->search_request_pending_;
  }

  void SetSearchRequestPending(bool search_request_pending) {
    amount_extraction_manager_->search_request_pending_ =
        search_request_pending;
  }

  bool IsTimeoutTimerRunning() {
    return amount_extraction_manager_->timeout_timer_.IsRunning();
  }

  void Reset() { amount_extraction_manager_->Reset(); }

  void SetAiAmountExtractionStartTime(base::TimeTicks time) {
    amount_extraction_manager_->ai_amount_extraction_start_time_ = time;
  }

 private:
  const raw_ref<AmountExtractionManager> amount_extraction_manager_;
};

inline AmountExtractionManagerTestApi test_api(
    AmountExtractionManager& manager) {
  return AmountExtractionManagerTestApi(&manager);
}

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AMOUNT_EXTRACTION_MANAGER_TEST_API_H_
