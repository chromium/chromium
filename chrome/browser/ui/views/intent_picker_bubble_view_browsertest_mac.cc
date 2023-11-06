// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/apps/link_capturing/mac_intent_picker_helpers.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/intent_picker_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/views/widget/any_widget_observer.h"

class IntentPickerBubbleViewBrowserTestMac : public InProcessBrowserTest {
 public:
  PageActionIconView* GetIntentPickerIcon() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar_button_provider()
        ->GetPageActionIconView(PageActionIconType::kIntentPicker);
  }

  IntentPickerBubbleView* intent_picker_bubble() {
    return IntentPickerBubbleView::intent_picker_bubble();
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  template <typename Action>
  void DoAndWaitForIntentPickerIconUpdate(Action action) {
    base::RunLoop run_loop;
    auto* tab_helper = IntentPickerTabHelper::FromWebContents(GetWebContents());
    tab_helper->SetIconUpdateCallbackForTesting(run_loop.QuitClosure());
    action();
    run_loop.Run();
  }

  size_t GetItemContainerSize(IntentPickerBubbleView* bubble) {
    return bubble->GetViewByID(IntentPickerBubbleView::ViewId::kItemContainer)
        ->children()
        .size();
  }

 private:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

// Test that if there is a Universal Link for a site, it shows the picker icon
// and lists the app as an option in the bubble.
IN_PROC_BROWSER_TEST_F(IntentPickerBubbleViewBrowserTestMac,
                       ShowUniversalLinkInIntentPicker) {
  const GURL url1(embedded_test_server()->GetURL("/title1.html"));
  const GURL url2(embedded_test_server()->GetURL("/title2.html"));

  const char* kFinderAppName = "Finder";
  const char* kFinderAppPath = "/System/Library/CoreServices/Finder.app";

  // Start with a page with no corresponding native app.

  DoAndWaitForIntentPickerIconUpdate([this, url1] {
    apps::OverrideMacAppForUrlForTesting(true, "");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));
  });

  // Verify that no icon was shown.

  ASSERT_FALSE(GetIntentPickerIcon()->GetVisible());

  // Load a different page while simulating it having a native app.

  DoAndWaitForIntentPickerIconUpdate([this, kFinderAppPath, url2] {
    apps::OverrideMacAppForUrlForTesting(true, kFinderAppPath);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));
  });

  // Verify that an icon was shown, but no bubble.

  ASSERT_TRUE(GetIntentPickerIcon()->GetVisible());
  EXPECT_FALSE(intent_picker_bubble());

  // Open the bubble.

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "IntentPickerBubbleView");
  GetIntentPickerIcon()->ExecuteForTesting();
  waiter.WaitIfNeededAndGet();

  // Verify the bubble contents.

  ASSERT_TRUE(intent_picker_bubble());
  EXPECT_TRUE(intent_picker_bubble()->GetVisible());

  EXPECT_EQ(1U, GetItemContainerSize(intent_picker_bubble()));
  auto& app_info = intent_picker_bubble()->app_info_for_testing();
  ASSERT_EQ(1U, app_info.size());
  EXPECT_EQ(kFinderAppPath, app_info[0].launch_name);
  EXPECT_EQ(kFinderAppName, app_info[0].display_name);

  // Navigate to the first page while simulating no native app.

  DoAndWaitForIntentPickerIconUpdate([this, url1] {
    apps::OverrideMacAppForUrlForTesting(true, "");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));
  });

  // Verify that the icon was hidden.

  ASSERT_FALSE(GetIntentPickerIcon()->GetVisible());
}
