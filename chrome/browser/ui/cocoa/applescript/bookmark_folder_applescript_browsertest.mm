// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/applescript/bookmark_applescript_test_utils.h"
#import "chrome/browser/ui/cocoa/applescript/bookmark_folder_applescript.h"
#import "chrome/browser/ui/cocoa/applescript/bookmark_item_applescript.h"
#import "chrome/browser/ui/cocoa/applescript/constants_applescript.h"
#import "chrome/browser/ui/cocoa/applescript/error_applescript.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

using BookmarkFolderAppleScriptTest = BookmarkAppleScriptTest;

namespace AppleScript {

namespace {

// Test all the bookmark folders within.
IN_PROC_BROWSER_TEST_F(BookmarkFolderAppleScriptTest, BookmarkFolders) {
  NSArray* bookmark_folders = bookmark_bar_.bookmarkFolders;

  EXPECT_EQ(2U, bookmark_folders.count);

  BookmarkFolderAppleScript* f1 = bookmark_folders[0];
  BookmarkFolderAppleScript* f2 = bookmark_folders[1];
  EXPECT_NSEQ(@"f1", f1.title);
  EXPECT_NSEQ(@"f2", f2.title);
  EXPECT_EQ(2, f1.index.intValue);
  EXPECT_EQ(4, f2.index.intValue);

  for (BookmarkFolderAppleScript* bookmark_folder in bookmark_folders) {
    EXPECT_EQ(bookmark_folder.container, bookmark_bar_);
    EXPECT_NSEQ(kBookmarkFoldersProperty, bookmark_folder.containerProperty);
  }
}

// Insert a new bookmark folder.
IN_PROC_BROWSER_TEST_F(BookmarkFolderAppleScriptTest, InsertBookmarkFolder) {
  // Emulate what AppleScript would do when inserting a new bookmark folder.
  // Emulates a script like |set var to make new bookmark folder with
  // properties {title:"foo"}|.
  BookmarkFolderAppleScript* bookmark_folder =
      [[BookmarkFolderAppleScript alloc] init];
  NSString* unique_id = [bookmark_folder.uniqueID copy];
  [bookmark_folder setTitle:@"foo"];
  [bookmark_bar_ insertInBookmarkFolders:bookmark_folder];

  // Represents the bookmark folder after it's added.
  BookmarkFolderAppleScript* bf = bookmark_bar_.bookmarkFolders[2];
  EXPECT_NSEQ(@"foo", bf.title);
  EXPECT_EQ(bf.container, bookmark_bar_);
  EXPECT_NSEQ(kBookmarkFoldersProperty, bf.containerProperty);
  EXPECT_NSEQ(unique_id, bf.uniqueID);
}

// Insert a new bookmark folder at a particular position.
IN_PROC_BROWSER_TEST_F(BookmarkFolderAppleScriptTest,
                       InsertBookmarkFolderAtPosition) {
  // Emulate what AppleScript would do when inserting a new bookmark folder.
  // Emulates a script like |set var to make new bookmark folder with
  // properties {title:"foo"} at after bookmark folder 1|.
  BookmarkFolderAppleScript* bookmark_folder =
      [[BookmarkFolderAppleScript alloc] init];
  NSString* unique_id = [bookmark_folder.uniqueID copy];
  bookmark_folder.title = @"foo";
  [bookmark_bar_ insertInBookmarkFolders:bookmark_folder atIndex:1];

  // Represents the bookmark folder after it's added.
  BookmarkFolderAppleScript* bf = bookmark_bar_.bookmarkFolders[1];
  EXPECT_NSEQ(@"foo", bf.title);
  EXPECT_EQ(bf.container, bookmark_bar_);
  EXPECT_NSEQ(kBookmarkFoldersProperty, bf.containerProperty);
  EXPECT_NSEQ(unique_id, bf.uniqueID);
}

// Delete bookmark folders.
IN_PROC_BROWSER_TEST_F(BookmarkFolderAppleScriptTest, DeleteBookmarkFolders) {
  unsigned int folder_count = 2, item_count = 3;
  for (unsigned int i = 0; i < folder_count; ++i) {
    EXPECT_EQ(folder_count - i, bookmark_bar_.bookmarkFolders.count);
    EXPECT_EQ(item_count, bookmark_bar_.bookmarkItems.count);
    [bookmark_bar_ removeFromBookmarkFoldersAtIndex:0];
  }
}

// Test all the bookmark items within.
IN_PROC_BROWSER_TEST_F(BookmarkFolderAppleScriptTest, BookmarkItems) {
  NSArray* bookmark_items = bookmark_bar_.bookmarkItems;

  EXPECT_EQ(3U, bookmark_items.count);

  BookmarkItemAppleScript* i1 = bookmark_items[0];
  BookmarkItemAppleScript* i2 = bookmark_items[1];
  BookmarkItemAppleScript* i3 = bookmark_items[2];
  EXPECT_NSEQ(@"a", i1.title);
  EXPECT_NSEQ(@"d", i2.title);
  EXPECT_NSEQ(@"h", i3.title);
  EXPECT_EQ(1, i1.index.intValue);
  EXPECT_EQ(3, i2.index.intValue);
  EXPECT_EQ(5, i3.index.intValue);

  for (BookmarkItemAppleScript* bookmark_item in bookmark_items) {
    EXPECT_EQ(bookmark_item.container, bookmark_bar_);
    EXPECT_NSEQ(kBookmarkItemsProperty, bookmark_item.containerProperty);
  }
}

// Insert a new bookmark item.
IN_PROC_BROWSER_TEST_F(BookmarkFolderAppleScriptTest, InsertBookmarkItem) {
  // Emulate what AppleScript would do when inserting a new bookmark folder.
  // Emulates a script like |set var to make new bookmark item with
  // properties {title:"Google", URL:"http://google.com"}|.
  BookmarkItemAppleScript* bookmark_item =
      [[BookmarkItemAppleScript alloc] init];
  NSString* unique_id = [bookmark_item.uniqueID copy];
  bookmark_item.title = @"Google";
  bookmark_item.URL = @"http://google.com";
  [bookmark_bar_ insertInBookmarkItems:bookmark_item];

  // Represents the bookmark item after it's added.
  BookmarkItemAppleScript* bi = bookmark_bar_.bookmarkItems[3];
  EXPECT_NSEQ(@"Google", bi.title);
  EXPECT_EQ(GURL("http://google.com/"), GURL(base::SysNSStringToUTF8(bi.URL)));
  EXPECT_EQ(bi.container, bookmark_bar_);
  EXPECT_NSEQ(kBookmarkItemsProperty, bi.containerProperty);
  EXPECT_NSEQ(unique_id, bi.uniqueID);

  // Test to see no bookmark item is created when no/invalid URL is entered.
  FakeScriptCommand* fake_script_command = [[FakeScriptCommand alloc] init];
  bookmark_item = [[BookmarkItemAppleScript alloc] init];
  [bookmark_bar_ insertInBookmarkItems:bookmark_item];
  EXPECT_EQ(static_cast<int>(Error::kInvalidURL),
            fake_script_command.scriptErrorNumber);
}

// Insert a new bookmark item at a particular position.
IN_PROC_BROWSER_TEST_F(BookmarkFolderAppleScriptTest,
                       InsertBookmarkItemAtPosition) {
  // Emulate what AppleScript would do when inserting a new bookmark item.
  // Emulates a script like:
  //
  //   set var to make new bookmark item with properties
  //       {title:"XKCD", URL:"http://xkcd.org} at after bookmark item 1
  BookmarkItemAppleScript* bookmark_item =
      [[BookmarkItemAppleScript alloc] init];
  NSString* unique_id = [bookmark_item.uniqueID copy];
  bookmark_item.title = @"XKCD";
  bookmark_item.URL = @"http://xkcd.org";

  [bookmark_bar_ insertInBookmarkItems:bookmark_item atIndex:1];

  // Represents the bookmark item after its added.
  BookmarkItemAppleScript* bi = bookmark_bar_.bookmarkItems[1];
  EXPECT_NSEQ(@"XKCD", bi.title);
  EXPECT_EQ(GURL("http://xkcd.org/"), GURL(base::SysNSStringToUTF8(bi.URL)));
  EXPECT_EQ(bi.container, bookmark_bar_);
  EXPECT_NSEQ(kBookmarkItemsProperty, bi.containerProperty);
  EXPECT_NSEQ(unique_id, bi.uniqueID);

  // Test to see no bookmark item is created when no/invalid URL is entered.
  FakeScriptCommand* fake_script_command = [[FakeScriptCommand alloc] init];
  bookmark_item = [[BookmarkItemAppleScript alloc] init];
  [bookmark_bar_ insertInBookmarkItems:bookmark_item atIndex:1];
  EXPECT_EQ(static_cast<int>(Error::kInvalidURL),
            fake_script_command.scriptErrorNumber);
}

// Delete bookmark items.
IN_PROC_BROWSER_TEST_F(BookmarkFolderAppleScriptTest, DeleteBookmarkItems) {
  unsigned int folder_count = 2, item_count = 3;
  for (unsigned int i = 0; i < item_count; ++i) {
    EXPECT_EQ(folder_count, bookmark_bar_.bookmarkFolders.count);
    EXPECT_EQ(item_count - i, bookmark_bar_.bookmarkItems.count);
    [bookmark_bar_ removeFromBookmarkItemsAtIndex:0];
  }
}

// Set and get title.
IN_PROC_BROWSER_TEST_F(BookmarkFolderAppleScriptTest, GetAndSetTitle) {
  NSArray* bookmark_folders = bookmark_bar_.bookmarkFolders;
  BookmarkFolderAppleScript* folder1 = bookmark_folders[0];
  folder1.title = @"Foo";
  EXPECT_NSEQ(@"Foo", folder1.title);
}

}  // namespace

}  // namespace AppleScript
