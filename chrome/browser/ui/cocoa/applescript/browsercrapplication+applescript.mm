// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/applescript/browsercrapplication+applescript.h"

#import "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"
#include "base/notreached.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#import "chrome/browser/ui/cocoa/applescript/bookmark_folder_applescript.h"
#import "chrome/browser/ui/cocoa/applescript/constants_applescript.h"
#import "chrome/browser/ui/cocoa/applescript/error_applescript.h"
#import "chrome/browser/ui/cocoa/applescript/window_applescript.h"
#include "components/bookmarks/browser/bookmark_model.h"

using bookmarks::BookmarkModel;

@implementation BrowserCrApplication (AppleScriptAdditions)

- (NSArray*)appleScriptWindows {
  NSMutableArray* appleScriptWindows = [NSMutableArray
      arrayWithCapacity:chrome::GetTotalBrowserCount()];
  // Iterate through all browsers and check if it closing,
  // if not add it to list.
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->IsAttemptingToCloseBrowser())
      continue;

    base::scoped_nsobject<WindowAppleScript> window(
        [[WindowAppleScript alloc] initWithBrowser:browser]);
    [window setContainer:NSApp
                property:AppleScript::kWindowsProperty];
    [appleScriptWindows addObject:window];
  }
  // Windows sorted by their index value, which is obtained by calling
  // orderedIndex: on each window.
  [appleScriptWindows sortUsingSelector:@selector(windowComparator:)];
  return appleScriptWindows;
}

- (void)insertInAppleScriptWindows:(WindowAppleScript*)aWindow {
  // This method gets called when a new window is created so
  // the container and property are set here.
  [aWindow setContainer:self
               property:AppleScript::kWindowsProperty];
}

- (void)insertInAppleScriptWindows:(WindowAppleScript*)aWindow
                           atIndex:(int)index {
  // This method gets called when a new window is created so
  // the container and property are set here.
  [aWindow setContainer:self
               property:AppleScript::kWindowsProperty];
  // Note: AppleScript is 1-based.
  index--;
  [aWindow setOrderedIndex:@(index)];
}

- (void)removeFromAppleScriptWindowsAtIndex:(int)index {
  [[self appleScriptWindows][index] handlesCloseScriptCommand:nil];
}

- (NSScriptObjectSpecifier*)objectSpecifier {
  return nil;
}

- (BookmarkFolderAppleScript*)otherBookmarks {
  AppController* appDelegate =
      base::mac::ObjCCastStrict<AppController>([NSApp delegate]);

  Profile* lastProfile = [appDelegate lastProfile];
  if (!lastProfile) {
    AppleScript::SetError(AppleScript::errGetProfile);
    return nil;
  }

  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(lastProfile);
  if (!model->loaded()) {
    AppleScript::SetError(AppleScript::errBookmarkModelLoad);
    return nil;
  }

  BookmarkFolderAppleScript* otherBookmarks =
      [[[BookmarkFolderAppleScript alloc]
          initWithBookmarkNode:model->other_node()] autorelease];
  [otherBookmarks setContainer:self
                      property:AppleScript::kBookmarkFoldersProperty];
  return otherBookmarks;
}

- (BookmarkFolderAppleScript*)bookmarksBar {
  AppController* appDelegate =
      base::mac::ObjCCastStrict<AppController>([NSApp delegate]);

  Profile* lastProfile = [appDelegate lastProfile];
  if (!lastProfile) {
    AppleScript::SetError(AppleScript::errGetProfile);
    return nil;
  }

  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(lastProfile);
  if (!model->loaded()) {
    AppleScript::SetError(AppleScript::errBookmarkModelLoad);
    return NULL;
  }

  BookmarkFolderAppleScript* bookmarksBar =
      [[[BookmarkFolderAppleScript alloc]
          initWithBookmarkNode:model->bookmark_bar_node()] autorelease];
  [bookmarksBar setContainer:self
                    property:AppleScript::kBookmarkFoldersProperty];
  return bookmarksBar;
}

- (NSArray*)bookmarkFolders {
  BookmarkFolderAppleScript* otherBookmarks = [self otherBookmarks];
  BookmarkFolderAppleScript* bookmarksBar = [self bookmarksBar];
  NSArray* folderArray = @[ otherBookmarks, bookmarksBar ];
  return folderArray;
}

- (void)insertInBookmarksFolders:(id)aBookmarkFolder {
  NOTIMPLEMENTED();
}

- (void)insertInBookmarksFolders:(id)aBookmarkFolder atIndex:(int)index {
  NOTIMPLEMENTED();
}

- (void)removeFromBookmarksFoldersAtIndex:(int)index {
  NOTIMPLEMENTED();
}

@end
