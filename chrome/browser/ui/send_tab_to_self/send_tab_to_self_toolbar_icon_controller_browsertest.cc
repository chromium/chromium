// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_toolbar_icon_controller.h"

#include "chrome/browser/send_tab_to_self/receiving_ui_handler_registry.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_toolbar_icon_controller_delegate.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
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

namespace {

class MockSendTabToSelfToolbarIconView : public SendTabToSelfToolbarIconView {
 public:
  explicit MockSendTabToSelfToolbarIconView(BrowserView* browser_view)
      : SendTabToSelfToolbarIconView(browser_view) {}

  MOCK_METHOD(void, Show, (const SendTabToSelfEntry& entry), (override));
};

}  // namespace

class SendTabToSelfToolbarIconControllerTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
    controller()->ClearDelegateListForTesting();
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
};

IN_PROC_BROWSER_TEST_F(SendTabToSelfToolbarIconControllerTest,
                       DisplayNewEntry) {
  MockSendTabToSelfToolbarIconView mock_icon(browser_view());
  ASSERT_TRUE(browser()->IsActive());

  SendTabToSelfEntry entry("a", GURL("http://www.example-a.com"), "a site",
                           base::Time(), "device a", "device b");

  EXPECT_CALL(mock_icon, Show(testing::_)).Times(1);
  controller()->DisplayNewEntries({&entry});
}

// This test cannot work on Wayland because the platform does not allow clients
// to position top level windows, activate them, and set focus.
#if !(BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE_WAYLAND))
IN_PROC_BROWSER_TEST_F(SendTabToSelfToolbarIconControllerTest,
                       StorePendingNewEntry) {
  MockSendTabToSelfToolbarIconView mock_icon(browser_view());
  ASSERT_TRUE(browser()->IsActive());

  Browser* incognito_browser = CreateIncognitoBrowser();
  WaitUntilBrowserBecomeActiveOrLastActive(incognito_browser);

  SendTabToSelfEntry entry("a", GURL("http://www.example-a.com"), "a site",
                           base::Time(), "device a", "device b");

  EXPECT_CALL(mock_icon, Show(testing::_)).Times(0);
  EXPECT_FALSE(browser()->IsActive());
  controller()->DisplayNewEntries({&entry});

  EXPECT_CALL(mock_icon, Show(testing::_)).Times(1);
  browser_view()->Activate();
  WaitUntilBrowserBecomeActiveOrLastActive(browser());
}
#endif

}  // namespace send_tab_to_self
