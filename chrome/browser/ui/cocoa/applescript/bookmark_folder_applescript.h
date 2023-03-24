// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_FOLDER_APPLESCRIPT_H_
#define CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_FOLDER_APPLESCRIPT_H_

#import <Foundation/Foundation.h>

#import "chrome/browser/ui/cocoa/applescript/bookmark_node_applescript.h"

@class BookmarkItemAppleScript;

// Represent a bookmark folder scriptable object in AppleScript.
@interface BookmarkFolderAppleScript : BookmarkNodeAppleScript

// Returns an array of all the bookmark folders contained within this particular
// folder.
@property(readonly) NSArray<BookmarkFolderAppleScript*>* bookmarkFolders;

// Returns an array of all the bookmark items contained within this particular
// folder.
@property(readonly) NSArray<BookmarkItemAppleScript*>* bookmarkItems;

// Bookmark folder manipulation methods.

// Inserts a bookmark folder at the end.
- (void)insertInBookmarkFolders:(BookmarkFolderAppleScript*)aBookmarkFolder;

// Inserts a bookmark folder at some position in the list.
// Called by AppleScript which takes care of bounds checking, make sure of it
// before calling directly.
- (void)insertInBookmarkFolders:(BookmarkFolderAppleScript*)aBookmarkFolder
                        atIndex:(size_t)index;

// Remove a bookmark folder from the list.
// Called by AppleScript which takes care of bounds checking, make sure of it
// before calling directly.
- (void)removeFromBookmarkFoldersAtIndex:(size_t)index;

// Bookmark item manipulation methods.

// Inserts a bookmark item at the end.
- (void)insertInBookmarkItems:(BookmarkItemAppleScript*)aBookmarkItem;

// Inserts a bookmark item at some position in the list.
// Called by AppleScript which takes care of bounds checking, make sure of it
// before calling directly.
- (void)insertInBookmarkItems:(BookmarkItemAppleScript*)aBookmarkItem
                      atIndex:(size_t)index;

// Removes a bookmarks folder from the list.
// Called by AppleScript which takes care of bounds checking, make sure of it
// before calling directly.
- (void)removeFromBookmarkItemsAtIndex:(size_t)index;

@end

#endif  // CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_FOLDER_APPLESCRIPT_H_
