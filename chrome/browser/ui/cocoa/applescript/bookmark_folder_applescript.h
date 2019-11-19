// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_FOLDER_APPLESCRIPT_H_
#define CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_FOLDER_APPLESCRIPT_H_

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/applescript/bookmark_node_applescript.h"

@class BookmarkItemAppleScript;

// Represent a bookmark folder scriptable object in applescript.
@interface BookmarkFolderAppleScript : BookmarkNodeAppleScript {

}

// Bookmark folder manipulation methods.
// Returns an array of |BookmarkFolderAppleScript*| of all the bookmark folders
// contained within this particular folder.
- (NSArray*)bookmarkFolders;

// Inserts a bookmark folder at the end.
- (void)insertInBookmarkFolders:(id)aBookmarkFolder;

// Inserts a bookmark folder at some position in the list.
// Called by applescript which takes care of bounds checking, make sure of it
// before calling directly.
- (void)insertInBookmarkFolders:(id)aBookmarkFolder atIndex:(size_t)index;

// Remove a bookmark folder from the list.
// Called by applescript which takes care of bounds checking, make sure of it
// before calling directly.
- (void)removeFromBookmarkFoldersAtIndex:(size_t)index;

// Bookmark item manipulation methods.
// Returns an array of |BookmarkItemAppleScript*| of all the bookmark items
// contained within this particular folder.
- (NSArray*)bookmarkItems;

// Inserts a bookmark item at the end.
- (void)insertInBookmarkItems:(BookmarkItemAppleScript*)aBookmarkItem;

// Inserts a bookmark item at some position in the list.
// Called by applescript which takes care of bounds checking, make sure of it
// before calling directly.
- (void)insertInBookmarkItems:(BookmarkItemAppleScript*)aBookmarkItem
                      atIndex:(size_t)index;

// Removes a bookmarks folder from the list.
// Called by applescript which takes care of bounds checking, make sure of it
// before calling directly.
- (void)removeFromBookmarkItemsAtIndex:(size_t)index;

// Returns the position of a bookmark folder within the current bookmark folder
// which consists of bookmark folders as well as bookmark items.
// AppleScript makes sure that there is a bookmark folder before calling this
// method, make sure of that before calling directly.
- (size_t)calculatePositionOfBookmarkFolderAt:(size_t)index;

// Returns the position of a bookmark item within the current bookmark folder
// which consists of bookmark folders as well as bookmark items.
// AppleScript makes sure that there is a bookmark item before calling this
// method, make sure of that before calling directly.
- (size_t)calculatePositionOfBookmarkItemAt:(size_t)index;

@end

#endif  // CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_FOLDER_APPLESCRIPT_H_
