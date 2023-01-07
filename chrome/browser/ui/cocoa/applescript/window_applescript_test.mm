// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/app_controller_mac.h"
#import "chrome/browser/chrome_browser_application_mac.h"
#include "chrome/browser/profiles/profile.h"
#import "chrome/browser/ui/cocoa/applescript/constants_applescript.h"
#import "chrome/browser/ui/cocoa/applescript/error_applescript.h"
#import "chrome/browser/ui/cocoa/applescript/tab_applescript.h"
#import "chrome/browser/ui/cocoa/applescript/window_applescript.h"
#include "chrome/browser/ui/cocoa/test/run_loop_testing.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "url/gurl.h"

typedef InProcessBrowserTest WindowAppleScriptTest;

// Create a window in default/normal mode.
IN_PROC_BROWSER_TEST_F(WindowAppleScriptTest, DefaultCreation) {
  base::scoped_nsobject<WindowAppleScript> aWindow(
      [[WindowAppleScript alloc] init]);
  EXPECT_TRUE(aWindow.get());
  NSString* mode = [aWindow.get() mode];
  EXPECT_NSEQ(AppleScript::kNormalWindowMode,
              mode);
}

// Create a window with a |NULL profile|.
IN_PROC_BROWSER_TEST_F(WindowAppleScriptTest, CreationWithNoProfile) {
  base::scoped_nsobject<WindowAppleScript> aWindow(
      [[WindowAppleScript alloc] initWithProfile:NULL]);
  EXPECT_FALSE(aWindow.get());
}

// Create a window with a particular profile.
IN_PROC_BROWSER_TEST_F(WindowAppleScriptTest, CreationWithProfile) {
  AppController* appController =
      base::mac::ObjCCastStrict<AppController>([NSApp delegate]);
  Profile* lastProfile = [appController lastProfile];
  base::scoped_nsobject<WindowAppleScript> aWindow(
      [[WindowAppleScript alloc] initWithProfile:lastProfile]);
  EXPECT_TRUE(aWindow.get());
  EXPECT_TRUE([aWindow.get() uniqueID]);
}

// Create a window with no |Browser*|.
IN_PROC_BROWSER_TEST_F(WindowAppleScriptTest, CreationWithNoBrowser) {
  base::scoped_nsobject<WindowAppleScript> aWindow(
      [[WindowAppleScript alloc] initWithBrowser:NULL]);
  EXPECT_FALSE(aWindow.get());
}

// Create a window with |Browser*| already present.
IN_PROC_BROWSER_TEST_F(WindowAppleScriptTest, CreationWithBrowser) {
  base::scoped_nsobject<WindowAppleScript> aWindow(
      [[WindowAppleScript alloc] initWithBrowser:browser()]);
  EXPECT_TRUE(aWindow.get());
  EXPECT_TRUE([aWindow.get() uniqueID]);
}

// Tabs within the window.
IN_PROC_BROWSER_TEST_F(WindowAppleScriptTest, Tabs) {
  base::scoped_nsobject<WindowAppleScript> aWindow(
      [[WindowAppleScript alloc] initWithBrowser:browser()]);
  NSArray* tabs = [aWindow.get() tabs];
  EXPECT_EQ(1U, [tabs count]);
  TabAppleScript* tab1 = tabs[0];
  EXPECT_EQ([tab1 container], aWindow.get());
  EXPECT_NSEQ(AppleScript::kTabsProperty,
              [tab1 containerProperty]);
}

// Insert a new tab.
IN_PROC_BROWSER_TEST_F(WindowAppleScriptTest, InsertTab) {
  // Emulate what applescript would do when creating a new tab.
  // Emulates a script like |set var to make new tab with
  // properties URL:"http://google.com"}|.
  base::scoped_nsobject<TabAppleScript> aTab([[TabAppleScript alloc] init]);
  base::scoped_nsobject<NSNumber> var([[aTab.get() uniqueID] copy]);
  [aTab.get() setURL:@"http://google.com"];
  base::scoped_nsobject<WindowAppleScript> aWindow(
      [[WindowAppleScript alloc] initWithBrowser:browser()]);
  [aWindow.get() insertInTabs:aTab.get()];

  // Represents the tab after it is inserted.
  TabAppleScript* tab = [aWindow.get() tabs][1];
  EXPECT_EQ(GURL("http://google.com"),
            GURL(base::SysNSStringToUTF8([tab URL])));
  EXPECT_EQ([tab container], aWindow.get());
  EXPECT_NSEQ(AppleScript::kTabsProperty,
              [tab containerProperty]);
  EXPECT_NSEQ(var.get(), [tab uniqueID]);
}

// Insert a new tab at a particular position
IN_PROC_BROWSER_TEST_F(WindowAppleScriptTest, InsertTabAtPosition) {
  // Emulate what applescript would do when creating a new tab.
  // Emulates a script like |set var to make new tab with
  // properties URL:"http://google.com"} at before tab 1|.
  base::scoped_nsobject<TabAppleScript> aTab([[TabAppleScript alloc] init]);
  base::scoped_nsobject<NSNumber> var([[aTab.get() uniqueID] copy]);
  [aTab.get() setURL:@"http://google.com"];
  base::scoped_nsobject<WindowAppleScript> aWindow(
      [[WindowAppleScript alloc] initWithBrowser:browser()]);
  [aWindow.get() insertInTabs:aTab.get() atIndex:0];

  // Represents the tab after it is inserted.
  TabAppleScript* tab = [aWindow.get() tabs][0];
  EXPECT_EQ(GURL("http://google.com"),
            GURL(base::SysNSStringToUTF8([tab URL])));
  EXPECT_EQ([tab container], aWindow.get());
  EXPECT_NSEQ(AppleScript::kTabsProperty, [tab containerProperty]);
  EXPECT_NSEQ(var.get(), [tab uniqueID]);
}

// Inserting and deleting tabs.
IN_PROC_BROWSER_TEST_F(WindowAppleScriptTest, InsertAndDeleteTabs) {
  base::scoped_nsobject<WindowAppleScript> aWindow(
      [[WindowAppleScript alloc] initWithBrowser:browser()]);
  base::scoped_nsobject<TabAppleScript> aTab;
  int count;
  for (int i = 0; i < 5; ++i) {
    for (int j = 0; j < 3; ++j) {
      aTab.reset([[TabAppleScript alloc] init]);
      [aWindow.get() insertInTabs:aTab.get()];
    }
    count = 3 * i + 4;
    EXPECT_EQ((int)[[aWindow.get() tabs] count], count);
  }

  count = (int)[[aWindow.get() tabs] count];
  for (int i = 0; i < 5; ++i) {
    for(int j = 0; j < 3; ++j) {
      [aWindow.get() removeFromTabsAtIndex:0];
    }
    count = count - 3;
    EXPECT_EQ((int)[[aWindow.get() tabs] count], count);
  }
}

// Getting and setting values from the NSWindow.
IN_PROC_BROWSER_TEST_F(WindowAppleScriptTest, NSWindowTest) {
  base::scoped_nsobject<WindowAppleScript> aWindow(
      [[WindowAppleScript alloc] initWithBrowser:browser()]);
  [aWindow.get() setValue:@YES forKey:@"isMiniaturized"];
  EXPECT_TRUE([[aWindow.get() valueForKey:@"isMiniaturized"] boolValue]);
  [aWindow.get() setValue:@NO forKey:@"isMiniaturized"];
  EXPECT_FALSE([[aWindow.get() valueForKey:@"isMiniaturized"] boolValue]);
}

// Getting and setting the active tab.
IN_PROC_BROWSER_TEST_F(WindowAppleScriptTest, ActiveTab) {
  base::scoped_nsobject<WindowAppleScript> aWindow(
      [[WindowAppleScript alloc] initWithBrowser:browser()]);
  base::scoped_nsobject<TabAppleScript> aTab([[TabAppleScript alloc] init]);
  [aWindow.get() insertInTabs:aTab.get()];
  [aWindow.get() setActiveTabIndex:@2];
  EXPECT_EQ(2, [[aWindow.get() activeTabIndex] intValue]);
  TabAppleScript* tab2 = [aWindow.get() tabs][1];
  EXPECT_NSEQ([[aWindow.get() activeTab] uniqueID],
              [tab2 uniqueID]);
}

// Order of windows.
IN_PROC_BROWSER_TEST_F(WindowAppleScriptTest, DISABLED_WindowOrder) {
  base::scoped_nsobject<WindowAppleScript> window2(
      [[WindowAppleScript alloc] initWithBrowser:browser()]);
  base::scoped_nsobject<WindowAppleScript> window1(
      [[WindowAppleScript alloc] init]);
  chrome::testing::NSRunLoopRunAllPending();
  EXPECT_EQ([window1.get() windowComparator:window2.get()], NSOrderedAscending);
  EXPECT_EQ([window2.get() windowComparator:window1.get()],
            NSOrderedDescending);
}
