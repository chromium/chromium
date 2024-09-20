// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_toolbar_icon_controller.h"

#include "chrome/browser/send_tab_to_self/receiving_ui_handler_registry.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_toolbar_icon_controller_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_bubble_controller.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_icon_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ozone_buildflags.h"

namespace send_tab_to_self {

class SendTabToSelfToolbarIconControllerTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  }

  void WaitUntilBrowserBecomeActiveOrLastActive(Browser* browser) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    ui_test_utils::WaitUntilBrowserBecomeActive(browser);
#else
    ui_test_utils::WaitForBrowserSetLastActive(browser);
#endif
  }

  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  SendTabToSelfToolbarIconController* controller() {
    return send_tab_to_self::ReceivingUiHandlerRegistry::GetInstance()
        ->GetToolbarButtonControllerForProfile(browser()->profile());
  }

  SendTabToSelfToolbarBubbleController* bubble_controller() {
    return browser()
        ->browser_window_features()
        ->send_tab_to_self_toolbar_bubble_controller();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{features::kToolbarPinning};
};

IN_PROC_BROWSER_TEST_F(SendTabToSelfToolbarIconControllerTest,
                       DisplayNewEntry) {
  ASSERT_TRUE(browser()->IsActive());

  SendTabToSelfEntry entry("a", GURL("http://www.example-a.com"), "a site",
                           base::Time(), "device a", "device b");

  controller()->DisplayNewEntries({&entry});
  EXPECT_TRUE(bubble_controller()->IsBubbleShowing());
}

// This test cannot work on Wayland because the platform does not allow clients
// to position top level windows, activate them, and set focus.
#if !(BUILDFLAG(IS_OZONE_WAYLAND) || BUILDFLAG(IS_CHROMEOS))
IN_PROC_BROWSER_TEST_F(SendTabToSelfToolbarIconControllerTest,
                       StorePendingNewEntry) {
  ASSERT_TRUE(browser()->IsActive());

  Browser* incognito_browser = CreateIncognitoBrowser();
  WaitUntilBrowserBecomeActiveOrLastActive(incognito_browser);

  SendTabToSelfEntry entry("a", GURL("http://www.example-a.com"), "a site",
                           base::Time(), "device a", "device b");

  EXPECT_FALSE(browser()->IsActive());
  controller()->DisplayNewEntries({&entry});
  EXPECT_FALSE(bubble_controller()->IsBubbleShowing());

  browser_view()->Activate();
  WaitUntilBrowserBecomeActiveOrLastActive(browser());
  EXPECT_TRUE(bubble_controller()->IsBubbleShowing());
}
#endif

IN_PROC_BROWSER_TEST_F(SendTabToSelfToolbarIconControllerTest,
                       ReplaceExistingEntry) {
  SendTabToSelfEntry existing_entry("a", GURL("http://www.example-a.com"),
                                    "a site", base::Time(), "device a",
                                    "device b");
  SendTabToSelfEntry new_entry("b", GURL("http://www.example-b.com"), "b site",
                               base::Time(), "device a", "device b");

  controller()->DisplayNewEntries({&existing_entry});
  EXPECT_EQ(existing_entry.GetGUID(),
            bubble_controller()->bubble()->GetGuidForTesting());

  // For some reason, displaying the initial bubble seems to deactivate the
  // browser
  browser_view()->Activate();
  WaitUntilBrowserBecomeActiveOrLastActive(browser());

  controller()->DisplayNewEntries({&new_entry});
  EXPECT_EQ(new_entry.GetGUID(),
            bubble_controller()->bubble()->GetGuidForTesting());
}

}  // namespace send_tab_to_self
