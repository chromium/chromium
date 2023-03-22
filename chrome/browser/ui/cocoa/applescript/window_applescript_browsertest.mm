// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

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

using WindowAppleScriptTest = InProcessBrowserTest;

// Create a window in default/normal mode.
IN_PROC_BROWSER_TEST_F(WindowAppleScriptTest, DefaultCreation) {
  base::scoped_nsobject<WindowAppleScript> window(
      [[WindowAppleScript alloc] init]);
  EXPECT_TRUE(window);
  NSString* mode = window.get().mode;
  EXPECT_NSEQ(AppleScript::kNormalWindowMode, mode);
}

// Create a window with a |NULL profile|.
IN_PROC_BROWSER_TEST_F(WindowAppleScriptTest, CreationWithNoProfile) {
  base::scoped_nsobject<WindowAppleScript> window(
      [[WindowAppleScript alloc] initWithProfile:nullptr]);
  EXPECT_FALSE(window);
}

// Create a window with a particular profile.
IN_PROC_BROWSER_TEST_F(WindowAppleScriptTest, CreationWithProfile) {
  AppController* appController =
      base::mac::ObjCCastStrict<AppController>(NSApp.delegate);
  Profile* lastProfile = appController.lastProfile;
  base::scoped_nsobject<WindowAppleScript> window(
      [[WindowAppleScript alloc] initWithProfile:lastProfile]);
  EXPECT_TRUE(window);
  EXPECT_TRUE(window.get().uniqueID);
}

// Create a window with no |Browser*|.
IN_PROC_BROWSER_TEST_F(WindowAppleScriptTest, CreationWithNoBrowser) {
  base::scoped_nsobject<WindowAppleScript> window(
      [[WindowAppleScript alloc] initWithBrowser:nullptr]);
  EXPECT_FALSE(window);
}

// Create a window with |Browser*| already present.
IN_PROC_BROWSER_TEST_F(WindowAppleScriptTest, CreationWithBrowser) {
  base::scoped_nsobject<WindowAppleScript> window(
      [[WindowAppleScript alloc] initWithBrowser:browser()]);
  EXPECT_TRUE(window);
  EXPECT_TRUE(window.get().uniqueID);
}

// Tabs within the window.
IN_PROC_BROWSER_TEST_F(WindowAppleScriptTest, Tabs) {
  base::scoped_nsobject<WindowAppleScript> window(
      [[WindowAppleScript alloc] initWithBrowser:browser()]);
  NSArray* tabs = window.get().tabs;
  EXPECT_EQ(1U, tabs.count);
  TabAppleScript* tab1 = tabs[0];
  EXPECT_EQ(tab1.container, window.get());
  EXPECT_NSEQ(AppleScript::kTabsProperty, tab1.containerProperty);
}

// Insert a new tab.
IN_PROC_BROWSER_TEST_F(WindowAppleScriptTest, InsertTab) {
  // Emulate what AppleScript would do when creating a new tab.
  // Emulates a script like:
  //
  //   set var to make new tab with properties {URL:"http://google.com"}
  base::scoped_nsobject<TabAppleScript> aTab([[TabAppleScript alloc] init]);
  base::scoped_nsobject<NSString> unique_id([aTab.get().uniqueID copy]);
  aTab.get().URL = @"http://google.com";
  base::scoped_nsobject<WindowAppleScript> window(
      [[WindowAppleScript alloc] initWithBrowser:browser()]);
  [window.get() insertInTabs:aTab.get()];

  // Represents the tab after it is inserted.
  TabAppleScript* tab = window.get().tabs[1];
  EXPECT_EQ(GURL("http://google.com"), GURL(base::SysNSStringToUTF8(tab.URL)));
  EXPECT_EQ(tab.container, window.get());
  EXPECT_NSEQ(AppleScript::kTabsProperty, tab.containerProperty);
  EXPECT_NSEQ(unique_id, tab.uniqueID);
}

// Insert a new tab at a particular position
IN_PROC_BROWSER_TEST_F(WindowAppleScriptTest, InsertTabAtPosition) {
  // Emulate what AppleScript would do when creating a new tab.
  // Emulates a script like:
  //
  //   set var to make new tab with properties
  //       {URL:"http://google.com"} at before tab 1
  base::scoped_nsobject<TabAppleScript> aTab([[TabAppleScript alloc] init]);
  base::scoped_nsobject<NSString> unique_id([aTab.get().uniqueID copy]);
  aTab.get().URL = @"http://google.com";
  base::scoped_nsobject<WindowAppleScript> window(
      [[WindowAppleScript alloc] initWithBrowser:browser()]);
  [window.get() insertInTabs:aTab.get() atIndex:0];

  // Represents the tab after it is inserted.
  TabAppleScript* tab = window.get().tabs[0];
  EXPECT_EQ(GURL("http://google.com"), GURL(base::SysNSStringToUTF8(tab.URL)));
  EXPECT_EQ(tab.container, window.get());
  EXPECT_NSEQ(AppleScript::kTabsProperty, tab.containerProperty);
  EXPECT_NSEQ(unique_id, tab.uniqueID);
}

// Inserting and deleting tabs.
IN_PROC_BROWSER_TEST_F(WindowAppleScriptTest, InsertAndDeleteTabs) {
  base::scoped_nsobject<WindowAppleScript> window(
      [[WindowAppleScript alloc] initWithBrowser:browser()]);
  base::scoped_nsobject<TabAppleScript> aTab;
  NSUInteger count;
  for (NSUInteger i = 0; i < 5; ++i) {
    for (NSUInteger j = 0; j < 3; ++j) {
      aTab.reset([[TabAppleScript alloc] init]);
      [window.get() insertInTabs:aTab.get()];
    }
    count = 3 * i + 4;
    EXPECT_EQ(window.get().tabs.count, count);
  }

  count = window.get().tabs.count;
  for (NSUInteger i = 0; i < 5; ++i) {
    for (NSUInteger j = 0; j < 3; ++j) {
      [window.get() removeFromTabsAtIndex:0];
    }
    count = count - 3;
    EXPECT_EQ(window.get().tabs.count, count);
  }
}

// Getting and setting values from the NSWindow.
IN_PROC_BROWSER_TEST_F(WindowAppleScriptTest, NSWindowTest) {
  base::scoped_nsobject<WindowAppleScript> window(
      [[WindowAppleScript alloc] initWithBrowser:browser()]);
  [window.get() setValue:@YES forKey:@"miniaturized"];
  EXPECT_TRUE([[window.get() valueForKey:@"miniaturized"] boolValue]);
  [window.get() setValue:@NO forKey:@"miniaturized"];
  EXPECT_FALSE([[window.get() valueForKey:@"miniaturized"] boolValue]);
}

// Getting and setting the active tab.
IN_PROC_BROWSER_TEST_F(WindowAppleScriptTest, ActiveTab) {
  base::scoped_nsobject<WindowAppleScript> window(
      [[WindowAppleScript alloc] initWithBrowser:browser()]);
  base::scoped_nsobject<TabAppleScript> aTab([[TabAppleScript alloc] init]);
  [window insertInTabs:aTab];
  [window setActiveTabIndex:@2];
  EXPECT_EQ(2, window.get().activeTabIndex.intValue);
  TabAppleScript* tab2 = window.get().tabs[1];
  EXPECT_NSEQ(window.get().activeTab.uniqueID, tab2.uniqueID);
}
