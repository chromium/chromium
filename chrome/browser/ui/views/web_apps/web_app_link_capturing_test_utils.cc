// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_link_capturing_test_utils.h"

#include "base/check_is_test.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/apps/link_capturing/link_capturing_features.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/intent_picker_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/intent_chip_button.h"
#include "chrome/common/chrome_features.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/widget/any_widget_observer.h"

namespace web_app {

IntentChipButton* GetIntentPickerIcon(Browser* browser) {
  CHECK(apps::features::ShouldShowLinkCapturingUX());
  return BrowserView::GetBrowserViewForBrowser(browser)
      ->toolbar_button_provider()
      ->GetIntentChipButton();
}

IntentPickerBubbleView* intent_picker_bubble() {
  return IntentPickerBubbleView::intent_picker_bubble();
}

testing::AssertionResult AwaitIntentPickerTabHelperIconUpdateComplete(
    content::WebContents* web_contents) {
  base::test::TestFuture<void> future;
  auto* tab_helper = IntentPickerTabHelper::FromWebContents(web_contents);
  tab_helper->SetIconUpdateCallbackForTesting(  // IN-TEST
      future.GetCallback(), /*include_latest_navigation=*/true);
  if (!future.Wait()) {
    return testing::AssertionFailure()
           << "Intent picker icon did not resolve an applicable app.";
  }
  return testing::AssertionSuccess();
}

testing::AssertionResult WaitForIntentPickerToShow(Browser* browser) {
  auto result = AwaitIntentPickerTabHelperIconUpdateComplete(
      browser->tab_strip_model()->GetActiveWebContents());
  if (!result) {
    return result;
  }
  IntentChipButton* intent_picker_icon = GetIntentPickerIcon(browser);
  if (!intent_picker_icon) {
    return testing::AssertionFailure() << "Intent picker icon does not exist.";
  }

  if (!intent_picker_icon->GetVisible()) {
    IntentChipVisibilityObserver(intent_picker_icon).WaitForChipToBeVisible();
    if (!intent_picker_icon->GetVisible()) {
      return testing::AssertionFailure()
             << "Intent picker icon never became visible.";
    }
  }

  return testing::AssertionSuccess();
}

testing::AssertionResult ClickIntentPickerChip(Browser* browser) {
  auto result = WaitForIntentPickerToShow(browser);
  if (!result) {
    return result;
  }

  views::test::ButtonTestApi test_api(GetIntentPickerIcon(browser));
  test_api.NotifyClick(ui::MouseEvent(
      ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  return testing::AssertionSuccess();
}

testing::AssertionResult ClickIntentPickerAndWaitForBubble(Browser* browser) {
  views::NamedWidgetShownWaiter intent_picker_bubble_shown(
      views::test::AnyWidgetTestPasskey{},
      IntentPickerBubbleView::kViewClassName);
  auto intent_chip_click_result = ClickIntentPickerChip(browser);
  if (!intent_chip_click_result) {
    return intent_chip_click_result;
  }

  if (!intent_picker_bubble_shown.WaitIfNeededAndGet()) {
    return testing::AssertionFailure()
           << "Intent picker bubble did not appear after click.";
  }

  EXPECT_NE(intent_picker_bubble(), nullptr) << "intent picker not initialized";
  return testing::AssertionSuccess();
}

views::Button* GetIntentPickerButtonAtIndex(size_t index) {
  EXPECT_NE(intent_picker_bubble(), nullptr)
      << " intent picker bubble not initialized";
  auto children =
      intent_picker_bubble()
          ->GetViewByID(IntentPickerBubbleView::ViewId::kItemContainer)
          ->children();
  EXPECT_LE(index, children.size());
  return static_cast<views::Button*>(children[index]);
}

IntentChipVisibilityObserver::IntentChipVisibilityObserver(
    IntentChipButton* intent_chip) {
  CHECK_IS_TEST();
  observation_.Observe(intent_chip);
}

IntentChipVisibilityObserver::~IntentChipVisibilityObserver() = default;

void IntentChipVisibilityObserver::WaitForChipToBeVisible() {
  run_loop_.Run();
}

void IntentChipVisibilityObserver::OnChipVisibilityChanged(bool is_visible) {
  if (is_visible) {
    run_loop_.Quit();
  }
}

}  // namespace web_app
