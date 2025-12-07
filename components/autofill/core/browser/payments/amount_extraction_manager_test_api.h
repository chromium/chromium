// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AMOUNT_EXTRACTION_MANAGER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AMOUNT_EXTRACTION_MANAGER_TEST_API_H_

#include <memory>

#include "base/check_deref.h"
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

  bool GetSearchRequestPending() {
    return amount_extraction_manager_->search_request_pending_;
  }

  void SetSearchRequestPending(bool search_request_pending) {
    amount_extraction_manager_->search_request_pending_ =
        search_request_pending;
  }

  bool GetIsFetchingAiPageContent() {
    return amount_extraction_manager_->is_fetching_ai_page_content_;
  }

  void SetIsFetchingAiPageContent(bool is_fetching) {
    amount_extraction_manager_->is_fetching_ai_page_content_ = is_fetching;
  }

  const optimization_guide::proto::AnnotatedPageContent* GetAiPageContent()
      const {
    return amount_extraction_manager_->ai_page_content_.get();
  }

  void SetAiPageContent() {
    amount_extraction_manager_->ai_page_content_ =
        std::make_unique<optimization_guide::proto::AnnotatedPageContent>(
            optimization_guide::proto::AnnotatedPageContent());
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
