// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/applescript/browsercrapplication+applescript.h"

#include <Foundation/Foundation.h>

#include <map>

#import "base/apple/foundation_util.h"
#include "base/notreached.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/applescript/bookmark_folder_applescript.h"
#import "chrome/browser/ui/cocoa/applescript/constants_applescript.h"
#import "chrome/browser/ui/cocoa/applescript/error_applescript.h"
#import "chrome/browser/ui/cocoa/applescript/window_applescript.h"
#include "components/bookmarks/browser/bookmark_model.h"

using bookmarks::BookmarkModel;

@implementation BrowserCrApplication (AppleScriptAdditions)

- (NSArray*)appleScriptWindows {
  std::map<NSWindow*, Browser*> browsers;
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->IsAttemptingToCloseBrowser()) {
      continue;
    }

    browsers.emplace(browser->window()->GetNativeWindow().GetNativeNSWindow(),
                     browser);
  }

  NSMutableArray* result = [NSMutableArray array];
  for (NSWindow* window in NSApp.orderedWindows) {
    const auto& browser_it = browsers.find(window);
    if (browser_it == browsers.end()) {
      continue;
    }

    WindowAppleScript* aWindow =
        [[WindowAppleScript alloc] initWithBrowser:browser_it->second];
    [aWindow setContainer:self property:AppleScript::kWindowsProperty];
    [result addObject:aWindow];
  }

  return result;
}

- (void)insertInAppleScriptWindows:(WindowAppleScript*)aWindow {
  // This method gets called when a new window is created so
  // the container and property are set here.
  [aWindow setContainer:self property:AppleScript::kWindowsProperty];
}

- (void)insertInAppleScriptWindows:(WindowAppleScript*)aWindow
                           atIndex:(int)index {
  // This method gets called when a new window is created so
  // the container and property are set here.
  [aWindow setContainer:self property:AppleScript::kWindowsProperty];
  // Note: AppleScript is 1-based.
  index--;
  aWindow.orderedIndex = @(index);
}

- (void)removeFromAppleScriptWindowsAtIndex:(int)index {
  [self.appleScriptWindows[index] handlesCloseScriptCommand:nil];
}

- (NSScriptObjectSpecifier*)objectSpecifier {
  return nil;
}

- (BookmarkFolderAppleScript*)otherBookmarks {
  Profile* lastProfile = AppController.sharedController.lastProfile;
  if (!lastProfile) {
    AppleScript::SetError(AppleScript::Error::kGetProfile);
    return nil;
  }

  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(lastProfile);
  if (!model->loaded()) {
    AppleScript::SetError(AppleScript::Error::kBookmarkModelLoad);
    return nil;
  }

  BookmarkFolderAppleScript* otherBookmarks = [[BookmarkFolderAppleScript alloc]
      initWithBookmarkNode:model->other_node()];
  [otherBookmarks setContainer:self
                      property:AppleScript::kBookmarkFoldersProperty];
  return otherBookmarks;
}

- (BookmarkFolderAppleScript*)bookmarksBar {
  Profile* lastProfile = AppController.sharedController.lastProfile;
  if (!lastProfile) {
    AppleScript::SetError(AppleScript::Error::kGetProfile);
    return nil;
  }

  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(lastProfile);
  if (!model->loaded()) {
    AppleScript::SetError(AppleScript::Error::kBookmarkModelLoad);
    return nullptr;
  }

  BookmarkFolderAppleScript* bookmarksBar = [[BookmarkFolderAppleScript alloc]
      initWithBookmarkNode:model->bookmark_bar_node()];
  [bookmarksBar setContainer:self
                    property:AppleScript::kBookmarkFoldersProperty];
  return bookmarksBar;
}

- (NSArray<BookmarkFolderAppleScript*>*)bookmarkFolders {
  return @[ self.otherBookmarks, self.bookmarksBar ];
}

- (void)insertInBookmarksFolders:(BookmarkFolderAppleScript*)aBookmarkFolder {
  NOTIMPLEMENTED();
}

- (void)insertInBookmarksFolders:(BookmarkFolderAppleScript*)aBookmarkFolder
                         atIndex:(int)index {
  NOTIMPLEMENTED();
}

- (void)removeFromBookmarksFoldersAtIndex:(int)index {
  NOTIMPLEMENTED();
}

@end
