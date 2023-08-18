// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include "base/apple/foundation_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/cocoa/applescript/bookmark_folder_applescript.h"
#import "chrome/browser/ui/cocoa/applescript/browsercrapplication+applescript.h"
#import "chrome/browser/ui/cocoa/applescript/constants_applescript.h"
#import "chrome/browser/ui/cocoa/applescript/window_applescript.h"
#include "chrome/browser/ui/cocoa/test/run_loop_testing.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "ui/gfx/geometry/size.h"

using BrowserCrApplicationAppleScriptTest = InProcessBrowserTest;

// Create windows of different |Type|.
IN_PROC_BROWSER_TEST_F(BrowserCrApplicationAppleScriptTest, Creation) {
  // Create additional |Browser*| objects of different type.
  Profile* profile = browser()->profile();
  Browser* b1 = Browser::Create(Browser::CreateParams(
      Browser::TYPE_POPUP, profile, /*user_gesture=*/true));
  Browser* b2 = Browser::Create(Browser::CreateParams::CreateForApp(
      "Test", /*trusted_source=*/true, gfx::Rect(), profile,
      /*user_gesture=*/true));

  EXPECT_EQ(3U, [NSApp appleScriptWindows].count);
  for (WindowAppleScript* window in [NSApp appleScriptWindows]) {
    EXPECT_NSEQ(AppleScript::kWindowsProperty, window.containerProperty);
    EXPECT_NSEQ(NSApp, window.container);
  }

  // Close the additional browsers.
  b1->tab_strip_model()->CloseAllTabs();
  b2->tab_strip_model()->CloseAllTabs();
}

// Insert a new window.
IN_PROC_BROWSER_TEST_F(BrowserCrApplicationAppleScriptTest,
                       DISABLED_InsertWindow) {
  // Emulate what AppleScript would do when creating a new window.
  // Emulate a script like:
  //
  //   set var to make new window with properties {visible:false}|.
  WindowAppleScript* aWindow = [[WindowAppleScript alloc] init];
  NSString* unique_id = [aWindow.uniqueID copy];
  [aWindow setValue:@YES forKey:@"visible"];

  [NSApp insertInAppleScriptWindows:aWindow];
  chrome::testing::NSRunLoopRunAllPending();

  // Represents the window after it is added.
  WindowAppleScript* window = [NSApp appleScriptWindows][0];
  EXPECT_NSEQ(@YES, [aWindow valueForKey:@"visible"]);
  EXPECT_EQ(window.container, NSApp);
  EXPECT_NSEQ(AppleScript::kWindowsProperty, window.containerProperty);
  EXPECT_NSEQ(unique_id, window.uniqueID);
}

// Inserting and deleting windows.
IN_PROC_BROWSER_TEST_F(BrowserCrApplicationAppleScriptTest,
                       InsertAndDeleteWindows) {
  WindowAppleScript* aWindow;
  NSUInteger count;
  // Create a bunch of windows.
  for (NSUInteger i = 0; i < 5; ++i) {
    for (NSUInteger j = 0; j < 3; ++j) {
      aWindow = [[WindowAppleScript alloc] init];
      [NSApp insertInAppleScriptWindows:aWindow];
    }
    count = 3 * i + 4;
    EXPECT_EQ(count, [NSApp appleScriptWindows].count);
  }

  // Remove all the windows, just created.
  count = [NSApp appleScriptWindows].count;
  for (NSUInteger i = 0; i < 5; ++i) {
    for (NSUInteger j = 0; j < 3; ++j) {
      [NSApp removeFromAppleScriptWindowsAtIndex:0];
    }
    count = count - 3;
    EXPECT_EQ(count, [NSApp appleScriptWindows].count);
  }
}

// Check for object specifier of the root scripting object.
IN_PROC_BROWSER_TEST_F(BrowserCrApplicationAppleScriptTest, ObjectSpecifier) {
  // Should always return nil to indicate its the root scripting object.
  EXPECT_EQ(nil, [NSApp objectSpecifier]);
}

// Bookmark folders at the root level.
IN_PROC_BROWSER_TEST_F(BrowserCrApplicationAppleScriptTest, BookmarkFolders) {
  NSArray* bookmark_folders = [NSApp bookmarkFolders];
  EXPECT_EQ(2U, bookmark_folders.count);

  for (BookmarkFolderAppleScript* bookmark_folder in bookmark_folders) {
    EXPECT_EQ(NSApp, bookmark_folder.container);
    EXPECT_NSEQ(AppleScript::kBookmarkFoldersProperty,
                bookmark_folder.containerProperty);
  }

  BookmarkFolderAppleScript* other_bookmarks =
      base::apple::ObjCCast<BookmarkFolderAppleScript>([NSApp otherBookmarks]);
  EXPECT_NSEQ(@"Other Bookmarks", other_bookmarks.title);
  BookmarkFolderAppleScript* bookmarks_bar =
      base::apple::ObjCCast<BookmarkFolderAppleScript>([NSApp bookmarksBar]);
  EXPECT_NSEQ(@"Bookmarks Bar", bookmarks_bar.title);
}
