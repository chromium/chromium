// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_INTENT_CHIP_BUTTON_TEST_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_INTENT_CHIP_BUTTON_TEST_BASE_H_

#include <string>

#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_test_accessor.h"

class Browser;

class IntentChipButtonTestBase {
 public:
  virtual ~IntentChipButtonTestBase() = default;

  // Checks if the intent chip is fully collapsed.
  bool IsIntentChipFullyCollapsed(Browser* browser);

  // Gets the intent chip accessor.
  page_actions::PageActionTestAccessor GetIntentChip(Browser* browser) const;

  // Check if the intent picker chip is done animating
  testing::AssertionResult WaitForPageActionButtonVisible(
      Browser* browser) const;

  // Function to generate test names for IntentChipButton tests.
  static std::string GenerateIntentChipTestName(
      const testing::TestParamInfo<apps::test::LinkCapturingFeatureVersion>&
          param_info);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_INTENT_CHIP_BUTTON_TEST_BASE_H_
