// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_link_capturing_test_utils.h"

#include "base/check_is_test.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/intent_picker_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view_interface.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_test_accessor.h"
#include "chrome/browser/web_applications/link_capturing_features.h"
#include "chrome/common/chrome_features.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/widget/any_widget_observer.h"

namespace web_app {

page_actions::PageActionTestAccessor GetIntentPickerButton(
    BrowserWindowInterface* browser) {
  return page_actions::PageActionTestAccessor(browser, kActionShowIntentPicker);
}

IntentPickerBubbleView* intent_picker_bubble() {
  return IntentPickerBubbleView::intent_picker_bubble();
}

testing::AssertionResult AwaitIntentPickerTabHelperIconUpdateComplete(
    content::WebContents* web_contents) {
  base::test::TestFuture<void> future;
  auto* tab_helper = IntentPickerTabHelper::From(
      tabs::TabInterface::GetFromContents(web_contents));
  tab_helper->SetIconUpdateCallbackForTesting(  // IN-TEST
      future.GetCallback(), /*include_latest_navigation=*/true);
  if (!future.Wait()) {
    return testing::AssertionFailure()
           << "Intent picker icon did not resolve an applicable app.";
  }
  return testing::AssertionSuccess();
}

testing::AssertionResult WaitForIntentPickerToShow(
    BrowserWindowInterface* browser) {
  auto result = AwaitIntentPickerTabHelperIconUpdateComplete(
      browser->GetActiveTabInterface()->GetContents());
  if (!result) {
    return result;
  }
  if (!GetIntentPickerButton(browser).GetVisible()) {
    return testing::AssertionFailure() << "Intent picker icon does not exist.";
  }

  return testing::AssertionSuccess();
}

testing::AssertionResult ClickIntentPickerChip(
    BrowserWindowInterface* browser) {
  testing::AssertionResult result = WaitForIntentPickerToShow(browser);

  if (!result) {
    return result;
  }

  GetIntentPickerButton(browser).Click();
  return testing::AssertionSuccess();
}

testing::AssertionResult ClickIntentPickerAndWaitForBubble(
    BrowserWindowInterface* browser) {
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

}  // namespace web_app
