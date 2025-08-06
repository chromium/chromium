// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_TEST_LENS_SEARCH_CONTEXTUALIZATION_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_TEST_LENS_SEARCH_CONTEXTUALIZATION_CONTROLLER_H_

#include "chrome/browser/ui/lens/lens_search_contextualization_controller.h"

namespace lens {

// Helper for testing features that use the
// LensSearchContextualizationController.
class TestLensSearchContextualizationController
    : public LensSearchContextualizationController {
 public:
  explicit TestLensSearchContextualizationController(
      LensSearchController* lens_search_controller);
  ~TestLensSearchContextualizationController() override;

  void CreatePageContextEligibilityAPI() override;

  bool GetCurrentPageContextEligibility() override;

  void ResetPageContextEligibilityAPI();

  // Sets the context eligibility of the page and creates the new API.
  void SetContextEligible(bool eligible);

 private:
  bool is_context_eligible_ = true;
  std::unique_ptr<optimization_guide::PageContextEligibilityAPI>
      page_context_eligibility_api_;
  std::unique_ptr<optimization_guide::PageContextEligibility>
      page_context_eligibility_holder_;
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_TEST_LENS_SEARCH_CONTEXTUALIZATION_CONTROLLER_H_
