// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/intent_chip_button_test_base.h"

#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/intent_chip_button.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "testing/gtest/include/gtest/gtest.h"

bool IntentChipButtonTestBase::IsMigrationEnabled() const {
  return base::FeatureList::IsEnabled(::features::kPageActionsMigration) &&
         ::features::kPageActionsMigrationIntentPicker.Get();
}

bool IntentChipButtonTestBase::IsIntentChipFullyCollapsed(Browser* browser) {
  if (IsMigrationEnabled()) {
    page_actions::PageActionView* page_action_view =
        static_cast<page_actions::PageActionView*>(GetIntentChip(browser));
    return !page_action_view->GetLabelForTesting() ||
           page_action_view->size() == page_action_view->GetMinimumSize();
  } else {
    IntentChipButton* intent_chip_button =
        static_cast<IntentChipButton*>(GetIntentChip(browser));
    return intent_chip_button->is_fully_collapsed();
  }
}

views::Button* IntentChipButtonTestBase::GetIntentChip(Browser* browser) {
  if (IsMigrationEnabled()) {
    return BrowserView::GetBrowserViewForBrowser(browser)
        ->toolbar_button_provider()
        ->GetPageActionView(kActionShowIntentPicker);
  }
  return BrowserView::GetBrowserViewForBrowser(browser)
      ->toolbar_button_provider()
      ->GetIntentChipButton();
}

testing::AssertionResult
IntentChipButtonTestBase::WaitForPageActionButtonVisible(
    Browser* browser) const {
  if (!IsMigrationEnabled()) {
    return testing::AssertionSuccess();
  }
  auto* view = BrowserView::GetBrowserViewForBrowser(browser)
                   ->toolbar_button_provider()
                   ->GetPageActionView(kActionShowIntentPicker);
  if (!view) {
    return testing::AssertionFailure();
  }

  bool is_view_visible = base::test::RunUntil(
      [&]() { return view->GetVisible() && !view->is_animating_label(); });

  return is_view_visible ? testing::AssertionSuccess()
                         : testing::AssertionFailure();
}

std::string IntentChipButtonTestBase::GenerateIntentChipTestName(
    const testing::TestParamInfo<
        std::tuple<apps::test::LinkCapturingFeatureVersion, bool>>&
        param_info) {
  std::string test_name;
  test_name.append(apps::test::ToString(
      std::get<apps::test::LinkCapturingFeatureVersion>(param_info.param)));
  test_name.append("_");
  if (std::get<bool>(param_info.param)) {
    test_name.append("page_action_on");
  } else {
    test_name.append("page_action_off");
  }
  return test_name;
}
