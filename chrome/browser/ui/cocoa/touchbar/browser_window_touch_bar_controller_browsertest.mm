// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/apple/foundation_util.h"
#include "base/apple/scoped_objc_class_swizzler.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/touchbar/browser_window_default_touch_bar.h"
#import "chrome/browser/ui/cocoa/touchbar/browser_window_touch_bar_controller.h"
#include "chrome/browser/ui/views/frame/browser_frame_mac.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/remote_cocoa/app_shim/window_touch_bar_delegate.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/search_engines_test_util.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_data_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest_mac.h"

// A class that watches for invalidations of a window's touch bar by calls
// to -setTouchBar:.
//
// Why is this class structured the way it is, with statics for the invalidation
// flag and the swizzler? We want to catch calls to -setTouchBar:. We can use
// swizzling for that. We implement -setTouchBar: in this class and swap it for
// the NSWindow implementation. Once inside TouchBarInvalidationWatcher's
// -setTouchBar: we need to set a flag somewhere to the value of (aTouchBar ==
// nil). In theory that flag could just be an instance variable of this class.
// The problem is when we reach -setTouchBar:, although the code comes from
// TouchBarInvalidationWatcher, the instance "executing" the code is an
// NSWindow. If we create an instance of TouchBarInvalidationWatcher that has a
// bool instance variable, say, the NSWindow can't access it (it's not the
// original TouchBarInvalidationWatcher instance). Similarly, at the end of
// -setTouchBar: we need to call the original (NSWindow) implementation of
// -setTouchBar:. To do that we need access to the ScopedObjCClassSwizzler that
// did the swap. If we store the ScopedObjCClassSwizzler in an instance variable
// of TouchBarInvalidationWatcher, our NSWindow instance again cannot access it.
//
// To get around these problems we store the flag and the swizzler in what are
// essentially class variables so they are accessible from anywhere.
@interface TouchBarInvalidationWatcher : NSObject
// Returns a new (non-autoreleased) TouchBarInvalidationWatcher.
+ (instancetype)newWatcher;

// Returns the touch bar invalidation flag. This flag is set to YES
// whenever -[NSWindow setTouchBar:] is called with nil.
+ (BOOL&)touchBarInvalidFlag;
@end

@implementation TouchBarInvalidationWatcher

+ (BOOL&)touchBarInvalidFlag {
  static BOOL touchBarInvalidFlag = NO;
  return touchBarInvalidFlag;
}

+ (std::unique_ptr<base::apple::ScopedObjCClassSwizzler>&)setTouchBarSwizzler {
  static base::NoDestructor<
      std::unique_ptr<base::apple::ScopedObjCClassSwizzler>>
      setTouchBarSwizzler(new base::apple::ScopedObjCClassSwizzler(
          [NSWindow class], [TouchBarInvalidationWatcher class],
          @selector(setTouchBar:)));

  return *setTouchBarSwizzler;
}

+ (instancetype)newWatcher {
  // Set up the swizzling.
  [self setTouchBarSwizzler];

  return [[TouchBarInvalidationWatcher alloc] init];
}

- (void)dealloc {
  [TouchBarInvalidationWatcher setTouchBarSwizzler].reset();
}

- (void)setTouchBar:(NSTouchBar*)aTouchBar {
  [TouchBarInvalidationWatcher touchBarInvalidFlag] = (aTouchBar == nil);

  // Proceed with setting the touch bar.
  [TouchBarInvalidationWatcher setTouchBarSwizzler]
      ->InvokeOriginal<void, NSTouchBar*>(self, @selector(setTouchBar:),
                                          aTouchBar);
}

@end

// A class that watches for page reload notifications in a window's touch bar.
//
// See the explanation at the top of TouchBarInvalidationWatcher for info on
// why this class is structured the way it is.
@interface PageReloadWatcher : NSObject
// Returns a new (non-autoreleased) PageReloadWatcher.
+ (instancetype)newWatcher;

// Returns the page loading flag. This flag is set to YES whenever
// -[BrowserWindowDefaultTouchBar setIsPageLoading:] is called with YES.
+ (BOOL&)pageIsLoadingFlag;
@end

@implementation PageReloadWatcher

+ (BOOL&)pageIsLoadingFlag {
  static BOOL pageIsLoadingFlag = NO;
  return pageIsLoadingFlag;
}

+ (std::unique_ptr<base::apple::ScopedObjCClassSwizzler>&)
    setPageIsLoadingSwizzler {
  static base::NoDestructor<
      std::unique_ptr<base::apple::ScopedObjCClassSwizzler>>
      setPageIsLoadingSwizzler(new base::apple::ScopedObjCClassSwizzler(
          [BrowserWindowDefaultTouchBar class], [PageReloadWatcher class],
          @selector(setIsPageLoading:)));

  return *setPageIsLoadingSwizzler;
}

+ (instancetype)newWatcher {
  // Set up the swizzling.
  [self setPageIsLoadingSwizzler];

  return [[PageReloadWatcher alloc] init];
}

- (void)dealloc {
  [PageReloadWatcher setPageIsLoadingSwizzler].reset();
}

- (void)setIsPageLoading:(BOOL)flag {
  if (flag) {
    [PageReloadWatcher pageIsLoadingFlag] = YES;
  }

  [PageReloadWatcher setPageIsLoadingSwizzler]->InvokeOriginal<void, BOOL>(
      self, @selector(setIsPageLoading:), flag);
}

@end

class BrowserWindowTouchBarControllerTest : public InProcessBrowserTest {
 public:
  BrowserWindowTouchBarControllerTest() = default;

  BrowserWindowTouchBarControllerTest(
      const BrowserWindowTouchBarControllerTest&) = delete;
  BrowserWindowTouchBarControllerTest& operator=(
      const BrowserWindowTouchBarControllerTest&) = delete;

  NSTouchBar* MakeTouchBar() {
    auto* delegate =
        static_cast<NSObject<WindowTouchBarDelegate>*>(native_window());
    return [delegate makeTouchBar];
  }

  NSWindow* native_window() const {
    return browser()->window()->GetNativeWindow().GetNativeNSWindow();
  }

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
};

// Test if the touch bar gets invalidated when the active tab is changed.
IN_PROC_BROWSER_TEST_F(BrowserWindowTouchBarControllerTest, TabChanges) {
  [[maybe_unused]] TouchBarInvalidationWatcher* invalidationWatcher =
      [TouchBarInvalidationWatcher newWatcher];

  EXPECT_FALSE(browser_touch_bar_controller());
  MakeTouchBar();
  EXPECT_TRUE(browser_touch_bar_controller());

  auto* current_touch_bar = [native_window() touchBar];
  EXPECT_TRUE(current_touch_bar);

  // Insert a new tab in the foreground. The window should invalidate its
  // touch bar as a result.
  [TouchBarInvalidationWatcher touchBarInvalidFlag] = NO;
  ASSERT_FALSE([TouchBarInvalidationWatcher touchBarInvalidFlag]);
  std::unique_ptr<content::WebContents> contents = content::WebContents::Create(
      content::WebContents::CreateParams(browser()->profile()));
  browser()->tab_strip_model()->AppendWebContents(std::move(contents), true);

  EXPECT_TRUE([TouchBarInvalidationWatcher touchBarInvalidFlag]);

  // Update the touch bar.
  [native_window() touchBar];

  // Activating the original tab should invalidate the touch bar.
  [TouchBarInvalidationWatcher touchBarInvalidFlag] = NO;
  ASSERT_FALSE([TouchBarInvalidationWatcher touchBarInvalidFlag]);
  browser()->tab_strip_model()->ActivateTabAt(0);

  EXPECT_TRUE([TouchBarInvalidationWatcher touchBarInvalidFlag]);
}

// Test if the touch bar receives a notification that the current tab is
// loading.
IN_PROC_BROWSER_TEST_F(BrowserWindowTouchBarControllerTest, PageReload) {
  [[maybe_unused]] PageReloadWatcher* pageReloadWatcher =
      [PageReloadWatcher newWatcher];

  EXPECT_FALSE(browser_touch_bar_controller());
  MakeTouchBar();
  EXPECT_TRUE(browser_touch_bar_controller());

  // Make sure the touch bar exists for the window.
  auto* current_touch_bar = [native_window() touchBar];
  EXPECT_TRUE(current_touch_bar);

  // We can't just ask the BrowserWindowDefaultTouchBar for the value of the
  // page loading flag like we can for the tab bookmark. The reload my happen
  // so fast that the flag may be reset to NO by the time we check it. We
  // have to use swizzling instead.
  [PageReloadWatcher pageIsLoadingFlag] = NO;
  ASSERT_FALSE([PageReloadWatcher pageIsLoadingFlag]);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html, <html><body></body></html>")));

  EXPECT_TRUE([PageReloadWatcher pageIsLoadingFlag]);
}

// Test if the touch bar receives a notification that the current tab has been
// bookmarked.
IN_PROC_BROWSER_TEST_F(BrowserWindowTouchBarControllerTest,
                       BookmarkCurrentTab) {
  EXPECT_FALSE(browser_touch_bar_controller());
  MakeTouchBar();
  EXPECT_TRUE(browser_touch_bar_controller());

  // Make sure the touch bar exists for the window.
  auto* current_touch_bar = [native_window() touchBar];
  EXPECT_TRUE(current_touch_bar);
  BrowserWindowDefaultTouchBar* touch_bar_delegate =
      base::apple::ObjCCastStrict<BrowserWindowDefaultTouchBar>(
          [current_touch_bar delegate]);
  EXPECT_FALSE([touch_bar_delegate isStarred]);

  chrome::BookmarkCurrentTab(browser());

  EXPECT_TRUE([touch_bar_delegate isStarred]);
}

// Tests if the touch bar's search button updates if the default search engine
// has changed.
IN_PROC_BROWSER_TEST_F(BrowserWindowTouchBarControllerTest,
                       SearchEngineChanges) {
  [[maybe_unused]] TouchBarInvalidationWatcher* invalidationWatcher =
      [TouchBarInvalidationWatcher newWatcher];

  PrefService* prefs = browser()->profile()->GetPrefs();
  DCHECK(prefs);

  EXPECT_FALSE(browser_touch_bar_controller());
  MakeTouchBar();

  // Force the window to create the touch bar.
  [native_window() touchBar];
  NSString* orig_search_button_title =
      [[[browser_touch_bar_controller() defaultTouchBar] searchButton] title];
  EXPECT_TRUE(orig_search_button_title);

  // Change the default search engine.
  [TouchBarInvalidationWatcher touchBarInvalidFlag] = NO;
  ASSERT_FALSE([TouchBarInvalidationWatcher touchBarInvalidFlag]);
  std::unique_ptr<TemplateURLData> data =
      GenerateDummyTemplateURLData("poutine");
  prefs->SetDict(DefaultSearchManager::kDefaultSearchProviderDataPrefName,
                 TemplateURLDataToDictionary(*data));

  // Confirm the touch bar was invalidated.
  EXPECT_TRUE([TouchBarInvalidationWatcher touchBarInvalidFlag]);

  // Ask the window again for its touch bar. Previously, changes like updates
  // to the default search engine would completely regenerate the touch bar.
  // That's expensive (view creation, autolayout, etc.). Instead we now retain
  // the original touch bar and expect touch bar invalidation to force an
  // update of the search item.
  [native_window() touchBar];
  EXPECT_FALSE([orig_search_button_title
      isEqualToString:[[[browser_touch_bar_controller() defaultTouchBar]
                          searchButton] title]]);
}

// Tests to see if the touch bar's bookmark tab helper observer gets removed
// when the touch bar is destroyed.
IN_PROC_BROWSER_TEST_F(BrowserWindowTouchBarControllerTest,
                       DestroyNotificationBridge) {
  MakeTouchBar();

  ASSERT_TRUE([browser_touch_bar_controller() defaultTouchBar]);

  BookmarkTabHelperObserver* observer =
      [[browser_touch_bar_controller() defaultTouchBar] bookmarkTabObserver];
  std::unique_ptr<content::WebContents> contents = content::WebContents::Create(
      content::WebContents::CreateParams(browser()->profile()));
  browser()->tab_strip_model()->AppendWebContents(std::move(contents), true);

  BookmarkTabHelper* tab_helper = BookmarkTabHelper::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(tab_helper);
  EXPECT_TRUE(tab_helper->HasObserver(observer));

  [[browser_touch_bar_controller() defaultTouchBar] setBrowser:nullptr];
  EXPECT_FALSE(tab_helper->HasObserver(observer));
}
