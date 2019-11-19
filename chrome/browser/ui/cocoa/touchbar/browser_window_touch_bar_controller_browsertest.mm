// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/touchbar/browser_window_default_touch_bar.h"
#import "chrome/browser/ui/cocoa/touchbar/browser_window_touch_bar_controller.h"
#include "chrome/browser/ui/views/frame/browser_frame_mac.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/remote_cocoa/app_shim/window_touch_bar_delegate.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/search_engines_test_util.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_data_util.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest_mac.h"

// TODO(spqchan): Write tests that will check for page load and bookmark
// updates.

class BrowserWindowTouchBarControllerTest : public InProcessBrowserTest {
 public:
  BrowserWindowTouchBarControllerTest() : InProcessBrowserTest() {}

  API_AVAILABLE(macos(10.12.2))
  NSTouchBar* MakeTouchBar() {
    auto* delegate =
        static_cast<NSObject<WindowTouchBarDelegate>*>(native_window());
    return [delegate makeTouchBar];
  }

  NSWindow* native_window() const {
    return browser()->window()->GetNativeWindow().GetNativeNSWindow();
  }

  API_AVAILABLE(macos(10.12.2))
  BrowserWindowTouchBarController* browser_touch_bar_controller() const {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForNativeWindow(native_window());
    EXPECT_TRUE(browser_view);
    if (!browser_view)
      return nil;

    BrowserFrameMac* browser_frame = static_cast<BrowserFrameMac*>(
        browser_view->frame()->native_browser_frame());
    return browser_frame->GetTouchBarController();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserWindowTouchBarControllerTest);
};

// Test if the touch bar gets invalidated when the active tab is changed.
IN_PROC_BROWSER_TEST_F(BrowserWindowTouchBarControllerTest, TabChanges) {
  if (@available(macOS 10.12.2, *)) {
    EXPECT_FALSE(browser_touch_bar_controller());
    MakeTouchBar();
    EXPECT_TRUE(browser_touch_bar_controller());

    auto* current_touch_bar = [native_window() touchBar];
    EXPECT_TRUE(current_touch_bar);

    // Insert a new tab in the foreground. The window should have a new touch
    // bar.
    std::unique_ptr<content::WebContents> contents =
        content::WebContents::Create(
            content::WebContents::CreateParams(browser()->profile()));
    browser()->tab_strip_model()->AppendWebContents(std::move(contents), true);
    EXPECT_NE(current_touch_bar, [native_window() touchBar]);
  }
}

// Tests if the touch bar gets invalidated if the default search engine has
// changed.
IN_PROC_BROWSER_TEST_F(BrowserWindowTouchBarControllerTest,
                       SearchEngineChanges) {
  if (@available(macOS 10.12.2, *)) {
    PrefService* prefs = browser()->profile()->GetPrefs();
    DCHECK(prefs);

    EXPECT_FALSE(browser_touch_bar_controller());
    MakeTouchBar();

    auto* current_touch_bar = [native_window() touchBar];
    EXPECT_TRUE(current_touch_bar);

    // Change the default search engine.
    std::unique_ptr<TemplateURLData> data =
        GenerateDummyTemplateURLData("poutine");
    prefs->Set(DefaultSearchManager::kDefaultSearchProviderDataPrefName,
               *TemplateURLDataToDictionary(*data));

    // The window should have a new touch bar.
    EXPECT_NE(current_touch_bar, [native_window() touchBar]);
  }
}

// Tests to see if the touch bar's bookmark tab helper observer gets removed
// when the touch bar is destroyed.
IN_PROC_BROWSER_TEST_F(BrowserWindowTouchBarControllerTest,
                       DestroyNotificationBridge) {
  if (@available(macOS 10.12.2, *)) {
    MakeTouchBar();

    ASSERT_TRUE([browser_touch_bar_controller() defaultTouchBar]);

    BookmarkTabHelperObserver* observer =
        [[browser_touch_bar_controller() defaultTouchBar] bookmarkTabObserver];
    std::unique_ptr<content::WebContents> contents =
        content::WebContents::Create(
            content::WebContents::CreateParams(browser()->profile()));
    browser()->tab_strip_model()->AppendWebContents(std::move(contents), true);

    BookmarkTabHelper* tab_helper = BookmarkTabHelper::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
    ASSERT_TRUE(tab_helper);
    EXPECT_TRUE(tab_helper->HasObserver(observer));

    CloseBrowserSynchronously(browser());

    EXPECT_FALSE(tab_helper->HasObserver(observer));
  }
}
