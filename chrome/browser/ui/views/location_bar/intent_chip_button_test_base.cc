// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/intent_chip_button_test_base.h"

#include "base/test/run_until.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_view_interface.h"
#include "testing/gtest/include/gtest/gtest.h"

bool IntentChipButtonTestBase::IsIntentChipFullyCollapsed(
    BrowserWindowInterface* browser) {
  return !GetIntentChip(browser).IsChipVisible();
}

page_actions::PageActionTestAccessor IntentChipButtonTestBase::GetIntentChip(
    BrowserWindowInterface* browser) const {
  return page_actions::PageActionTestAccessor(browser, kActionShowIntentPicker);
}

testing::AssertionResult
IntentChipButtonTestBase::WaitForPageActionButtonVisible(
    BrowserWindowInterface* browser) const {
  bool is_view_visible = base::test::RunUntil(
      [&]() { return GetIntentChip(browser).GetVisible(); });

  return is_view_visible ? testing::AssertionSuccess()
                         : testing::AssertionFailure();
}

std::string IntentChipButtonTestBase::GenerateIntentChipTestName(
    const testing::TestParamInfo<apps::test::LinkCapturingFeatureVersion>&
        param_info) {
  return apps::test::ToString(param_info.param) + "_page_action_on";
}
