// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/applescript/bookmark_applescript_utils_test.h"
#import "chrome/browser/ui/cocoa/applescript/bookmark_folder_applescript.h"
#import "chrome/browser/ui/cocoa/applescript/bookmark_item_applescript.h"
#import "chrome/browser/ui/cocoa/applescript/constants_applescript.h"
#import "chrome/browser/ui/cocoa/applescript/error_applescript.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

typedef BookmarkAppleScriptTest BookmarkFolderAppleScriptTest;

namespace {

// Test all the bookmark folders within.
IN_PROC_BROWSER_TEST_F(BookmarkFolderAppleScriptTest, BookmarkFolders) {
  NSArray* bookmarkFolders = [bookmarkBar_.get() bookmarkFolders];

  EXPECT_EQ(2U, [bookmarkFolders count]);

  BookmarkFolderAppleScript* f1 = bookmarkFolders[0];
  BookmarkFolderAppleScript* f2 = bookmarkFolders[1];
  EXPECT_NSEQ(@"f1", [f1 title]);
  EXPECT_NSEQ(@"f2", [f2 title]);
  EXPECT_EQ(2, [[f1 index] intValue]);
  EXPECT_EQ(4, [[f2 index] intValue]);

  for (BookmarkFolderAppleScript* bookmarkFolder in bookmarkFolders) {
    EXPECT_EQ([bookmarkFolder container], bookmarkBar_.get());
    EXPECT_NSEQ(AppleScript::kBookmarkFoldersProperty,
                [bookmarkFolder containerProperty]);
  }
}

// Insert a new bookmark folder.
IN_PROC_BROWSER_TEST_F(BookmarkFolderAppleScriptTest, InsertBookmarkFolder) {
  // Emulate what applescript would do when inserting a new bookmark folder.
  // Emulates a script like |set var to make new bookmark folder with
  // properties {title:"foo"}|.
  base::scoped_nsobject<BookmarkFolderAppleScript> bookmarkFolder(
      [[BookmarkFolderAppleScript alloc] init]);
  base::scoped_nsobject<NSNumber> var([[bookmarkFolder.get() uniqueID] copy]);
  [bookmarkFolder.get() setTitle:@"foo"];
  [bookmarkBar_.get() insertInBookmarkFolders:bookmarkFolder.get()];

  // Represents the bookmark folder after it's added.
  BookmarkFolderAppleScript* bf = [bookmarkBar_.get() bookmarkFolders][2];
  EXPECT_NSEQ(@"foo", [bf title]);
  EXPECT_EQ([bf container], bookmarkBar_.get());
  EXPECT_NSEQ(AppleScript::kBookmarkFoldersProperty,
              [bf containerProperty]);
  EXPECT_NSEQ(var.get(), [bf uniqueID]);
}

// Insert a new bookmark folder at a particular position.
IN_PROC_BROWSER_TEST_F(BookmarkFolderAppleScriptTest,
                       InsertBookmarkFolderAtPosition) {
  // Emulate what applescript would do when inserting a new bookmark folder.
  // Emulates a script like |set var to make new bookmark folder with
  // properties {title:"foo"} at after bookmark folder 1|.
  base::scoped_nsobject<BookmarkFolderAppleScript> bookmarkFolder(
      [[BookmarkFolderAppleScript alloc] init]);
  base::scoped_nsobject<NSNumber> var([[bookmarkFolder.get() uniqueID] copy]);
  [bookmarkFolder.get() setTitle:@"foo"];
  [bookmarkBar_.get() insertInBookmarkFolders:bookmarkFolder.get() atIndex:1];

  // Represents the bookmark folder after it's added.
  BookmarkFolderAppleScript* bf = [bookmarkBar_.get() bookmarkFolders][1];
  EXPECT_NSEQ(@"foo", [bf title]);
  EXPECT_EQ([bf container], bookmarkBar_.get());
  EXPECT_NSEQ(AppleScript::kBookmarkFoldersProperty, [bf containerProperty]);
  EXPECT_NSEQ(var.get(), [bf uniqueID]);
}

// Delete bookmark folders.
IN_PROC_BROWSER_TEST_F(BookmarkFolderAppleScriptTest, DeleteBookmarkFolders) {
  unsigned int folderCount = 2, itemCount = 3;
  for (unsigned int i = 0; i < folderCount; ++i) {
    EXPECT_EQ(folderCount - i, [[bookmarkBar_.get() bookmarkFolders] count]);
    EXPECT_EQ(itemCount, [[bookmarkBar_.get() bookmarkItems] count]);
    [bookmarkBar_.get() removeFromBookmarkFoldersAtIndex:0];
  }
}

// Test all the bookmark items within.
IN_PROC_BROWSER_TEST_F(BookmarkFolderAppleScriptTest, BookmarkItems) {
  NSArray* bookmarkItems = [bookmarkBar_.get() bookmarkItems];

  EXPECT_EQ(3U, [bookmarkItems count]);

  BookmarkItemAppleScript* i1 = bookmarkItems[0];
  BookmarkItemAppleScript* i2 = bookmarkItems[1];
  BookmarkItemAppleScript* i3 = bookmarkItems[2];
  EXPECT_NSEQ(@"a", [i1 title]);
  EXPECT_NSEQ(@"d", [i2 title]);
  EXPECT_NSEQ(@"h", [i3 title]);
  EXPECT_EQ(1, [[i1 index] intValue]);
  EXPECT_EQ(3, [[i2 index] intValue]);
  EXPECT_EQ(5, [[i3 index] intValue]);

  for (BookmarkItemAppleScript* bookmarkItem in bookmarkItems) {
    EXPECT_EQ([bookmarkItem container], bookmarkBar_.get());
    EXPECT_NSEQ(AppleScript::kBookmarkItemsProperty,
                [bookmarkItem containerProperty]);
  }
}

// Insert a new bookmark item.
IN_PROC_BROWSER_TEST_F(BookmarkFolderAppleScriptTest, InsertBookmarkItem) {
  // Emulate what applescript would do when inserting a new bookmark folder.
  // Emulates a script like |set var to make new bookmark item with
  // properties {title:"Google", URL:"http://google.com"}|.
  base::scoped_nsobject<BookmarkItemAppleScript> bookmarkItem(
      [[BookmarkItemAppleScript alloc] init]);
  base::scoped_nsobject<NSNumber> var([[bookmarkItem.get() uniqueID] copy]);
  [bookmarkItem.get() setTitle:@"Google"];
  [bookmarkItem.get() setURL:@"http://google.com"];
  [bookmarkBar_.get() insertInBookmarkItems:bookmarkItem.get()];

  // Represents the bookmark item after it's added.
  BookmarkItemAppleScript* bi = [bookmarkBar_.get() bookmarkItems][3];
  EXPECT_NSEQ(@"Google", [bi title]);
  EXPECT_EQ(GURL("http://google.com/"),
            GURL(base::SysNSStringToUTF8([bi URL])));
  EXPECT_EQ([bi container], bookmarkBar_.get());
  EXPECT_NSEQ(AppleScript::kBookmarkItemsProperty, [bi containerProperty]);
  EXPECT_NSEQ(var.get(), [bi uniqueID]);

  // Test to see no bookmark item is created when no/invlid URL is entered.
  base::scoped_nsobject<FakeScriptCommand> fakeScriptCommand(
      [[FakeScriptCommand alloc] init]);
  bookmarkItem.reset([[BookmarkItemAppleScript alloc] init]);
  [bookmarkBar_.get() insertInBookmarkItems:bookmarkItem.get()];
  EXPECT_EQ((int)AppleScript::errInvalidURL,
            [fakeScriptCommand.get() scriptErrorNumber]);
}

// Insert a new bookmark item at a particular position.
IN_PROC_BROWSER_TEST_F(BookmarkFolderAppleScriptTest,
                       InsertBookmarkItemAtPosition) {
  // Emulate what applescript would do when inserting a new bookmark item.
  // Emulates a script like |set var to make new bookmark item with
  // properties {title:"XKCD", URL:"http://xkcd.org}
  // at after bookmark item 1|.
  base::scoped_nsobject<BookmarkItemAppleScript> bookmarkItem(
      [[BookmarkItemAppleScript alloc] init]);
  base::scoped_nsobject<NSNumber> var([[bookmarkItem.get() uniqueID] copy]);
  [bookmarkItem.get() setTitle:@"XKCD"];
  [bookmarkItem.get() setURL:@"http://xkcd.org"];

  [bookmarkBar_.get() insertInBookmarkItems:bookmarkItem.get() atIndex:1];

  // Represents the bookmark item after its added.
  BookmarkItemAppleScript* bi = [bookmarkBar_.get() bookmarkItems][1];
  EXPECT_NSEQ(@"XKCD", [bi title]);
  EXPECT_EQ(GURL("http://xkcd.org/"),
            GURL(base::SysNSStringToUTF8([bi URL])));
  EXPECT_EQ([bi container], bookmarkBar_.get());
  EXPECT_NSEQ(AppleScript::kBookmarkItemsProperty,
              [bi containerProperty]);
  EXPECT_NSEQ(var.get(), [bi uniqueID]);

  // Test to see no bookmark item is created when no/invlid URL is entered.
  base::scoped_nsobject<FakeScriptCommand> fakeScriptCommand(
      [[FakeScriptCommand alloc] init]);
  bookmarkItem.reset([[BookmarkItemAppleScript alloc] init]);
  [bookmarkBar_.get() insertInBookmarkItems:bookmarkItem.get() atIndex:1];
  EXPECT_EQ((int)AppleScript::errInvalidURL,
            [fakeScriptCommand.get() scriptErrorNumber]);
}

// Delete bookmark items.
IN_PROC_BROWSER_TEST_F(BookmarkFolderAppleScriptTest, DeleteBookmarkItems) {
  unsigned int folderCount = 2, itemCount = 3;
  for (unsigned int i = 0; i < itemCount; ++i) {
    EXPECT_EQ(folderCount, [[bookmarkBar_.get() bookmarkFolders] count]);
    EXPECT_EQ(itemCount - i, [[bookmarkBar_.get() bookmarkItems] count]);
    [bookmarkBar_.get() removeFromBookmarkItemsAtIndex:0];
  }
}

// Set and get title.
IN_PROC_BROWSER_TEST_F(BookmarkFolderAppleScriptTest, GetAndSetTitle) {
  NSArray* bookmarkFolders = [bookmarkBar_.get() bookmarkFolders];
  BookmarkFolderAppleScript* folder1 = bookmarkFolders[0];
  [folder1 setTitle:@"Foo"];
  EXPECT_NSEQ(@"Foo", [folder1 title]);
}

}  // namespace
