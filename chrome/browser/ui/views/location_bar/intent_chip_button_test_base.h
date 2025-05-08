// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_INTENT_CHIP_BUTTON_TEST_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_INTENT_CHIP_BUTTON_TEST_BASE_H_

#include <string>

#include "base/feature_list.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/views/location_bar/intent_chip_button.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "ui/views/controls/button/button.h"

class Browser;

class IntentChipButtonTestBase {
 public:
  virtual ~IntentChipButtonTestBase() = default;

  // Checks if the intent chip is fully collapsed.
  bool IsIntentChipFullyCollapsed(Browser* browser);

  // Gets the intent chip button.
  views::Button* GetIntentChip(Browser* browser);

  // Check if the intent picker chip is done animating
  testing::AssertionResult WaitForPageActionButtonVisible(
      actions::ActionId action_id,
      Browser* browser) const;

  // Function to generate test names for IntentChipButton tests.
  static std::string GenerateIntentChipTestName(
      const testing::TestParamInfo<
          std::tuple<apps::test::LinkCapturingFeatureVersion, bool>>&
          param_info);

  // To check if migration has been enabled for page actions
  bool IsMigrationEnabled() const;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_INTENT_CHIP_BUTTON_TEST_BASE_H_
