// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/test_lens_search_contextualization_controller.h"

#include "chrome/browser/ui/lens/lens_search_contextualization_controller.h"

namespace lens {

TestLensSearchContextualizationController::
    TestLensSearchContextualizationController(
        LensSearchController* lens_search_controller)
    : LensSearchContextualizationController(lens_search_controller) {
  CreatePageContextEligibilityAPI();
}

TestLensSearchContextualizationController::
    ~TestLensSearchContextualizationController() {
  ResetPageContextEligibilityAPI();
}

void TestLensSearchContextualizationController::
    CreatePageContextEligibilityAPI() {
  // Reset any old API pointers that could be dangling from a previous
  // creation of the API.
  ResetPageContextEligibilityAPI();

  page_context_eligibility_api_ =
      std::make_unique<optimization_guide::PageContextEligibilityAPI>();

  page_context_eligibility_api_->IsPageContextEligible =
      is_context_eligible_
          ? [](const std::string& host, const std::string& path,
               const std::vector<
                   optimization_guide::FrameMetadata>&) { return true; }
          : [](const std::string& host, const std::string& path,
               const std::vector<optimization_guide::FrameMetadata>&) {
              return false;
            };

  page_context_eligibility_holder_ =
      std::make_unique<optimization_guide::PageContextEligibility>(
          page_context_eligibility_api_.get());
  page_context_eligibility_ = page_context_eligibility_holder_.get();
}

bool TestLensSearchContextualizationController::
    GetCurrentPageContextEligibility() {
  return is_context_eligible_;
}

void TestLensSearchContextualizationController::
    ResetPageContextEligibilityAPI() {
  page_context_eligibility_ = nullptr;
  page_context_eligibility_holder_.reset();
  page_context_eligibility_api_.reset();
}

// Sets the context eligibility of the page and creates the new API.
void TestLensSearchContextualizationController::SetContextEligible(
    bool eligible) {
  is_context_eligible_ = eligible;
  CreatePageContextEligibilityAPI();
}

}  // namespace lens
