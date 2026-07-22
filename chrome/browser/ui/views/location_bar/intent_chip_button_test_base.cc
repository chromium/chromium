// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/intent_chip_button_test_base.h"

#include "base/test/run_until.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_test_support.h"
#include "testing/gtest/include/gtest/gtest.h"

bool IntentChipButtonTestBase::IsIntentChipFullyCollapsed(Browser* browser) {
  page_actions::PageActionView* page_action_view =
      static_cast<page_actions::PageActionView*>(GetIntentChip(browser));
  return !page_action_view->GetLabelForTesting() ||
         page_action_view->size() == page_action_view->GetMinimumSize();
}

views::Button* IntentChipButtonTestBase::GetIntentChip(Browser* browser) {
  auto* provider =
      BrowserView::GetBrowserViewForBrowser(browser)->toolbar_button_provider();
  return page_actions::GetIconLabelBubbleViewForTesting(
      provider->GetPageActionViewInterface(kActionShowIntentPicker),
      kActionShowIntentPicker);
}

testing::AssertionResult
IntentChipButtonTestBase::WaitForPageActionButtonVisible(
    Browser* browser) const {
  auto* provider =
      BrowserView::GetBrowserViewForBrowser(browser)->toolbar_button_provider();
  auto* view = page_actions::GetIconLabelBubbleViewForTesting(
      provider->GetPageActionViewInterface(kActionShowIntentPicker),
      kActionShowIntentPicker);
  if (!view) {
    return testing::AssertionFailure();
  }

  bool is_view_visible = base::test::RunUntil(
      [&]() { return view->GetVisible() && !view->is_animating_label(); });

  return is_view_visible ? testing::AssertionSuccess()
                         : testing::AssertionFailure();
}

std::string IntentChipButtonTestBase::GenerateIntentChipTestName(
    const testing::TestParamInfo<apps::test::LinkCapturingFeatureVersion>&
        param_info) {
  return apps::test::ToString(param_info.param) + "_page_action_on";
}
