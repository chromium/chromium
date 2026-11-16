// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/applescript/browsercrapplication+applescript.h"

#include <Foundation/Foundation.h>

#import "base/apple/foundation_util.h"
#include "base/notimplemented.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#import "chrome/browser/ui/cocoa/applescript/bookmark_folder_applescript.h"
#import "chrome/browser/ui/cocoa/applescript/constants_applescript.h"
#import "chrome/browser/ui/cocoa/applescript/error_applescript.h"
#import "chrome/browser/ui/cocoa/applescript/window_applescript.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "ui/base/base_window.h"

using bookmarks::BookmarkModel;

@implementation BrowserCrApplication (AppleScriptAdditions)

- (NSArray<WindowAppleScript*>*)appleScriptWindows {
  return AppleScript::GetWindows();
}

- (void)insertInAppleScriptWindows:(WindowAppleScript*)aWindow {
  // AppleScript has already created the window object, so no work needs to be
  // done.
}

- (void)insertInAppleScriptWindows:(WindowAppleScript*)aWindow
                           atIndex:(int)index {
  // AppleScript has already created the window object, so move it to the
  // correct index. Note that while AppleScript is 1-based, this call is
  // scripting KVC, so that offset has already been handled and the index passed
  // in is 0-based.
  AppleScript::InsertWindowAtIndex(aWindow, index);
}

- (void)removeFromAppleScriptWindowsAtIndex:(int)index {
  // Closing a window is executed as a command; is this ever actually called?
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

@end
