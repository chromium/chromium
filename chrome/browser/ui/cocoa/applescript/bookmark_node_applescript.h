// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_NODE_APPLESCRIPT_H_
#define CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_NODE_APPLESCRIPT_H_

#import <Foundation/Foundation.h>

#include "base/uuid.h"
#import "chrome/browser/ui/cocoa/applescript/element_applescript.h"

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}

// Contains all the elements that are common to both a bookmark folder and
// bookmark item.
@interface BookmarkNodeAppleScript : ElementAppleScript

// Gets the node.
@property(readonly) const bookmarks::BookmarkNode* bookmarkNode;

// Get and Set title.
@property(copy) NSString* title;

// Returns the index with respect to its parent bookmark folder. 1-based because
// this is intended for use by AppleScript.
@property(readonly) NSNumber* index;

// Returns the bookmark model of the browser, returns null if there is an error.
@property(readonly) bookmarks::BookmarkModel* bookmarkModel;

// Returns the GUID of the bookmark node.
@property(readonly) base::Uuid bookmarkGUID;

// Does not actually create a folder/item but just sets its ID, the folder is
// created in insertInBookmarksFolder: in the corresponding bookmarks folder.
- (instancetype)init;

// Does not make a folder/item but instead uses an existing one.
- (instancetype)initWithBookmarkNode:
    (const bookmarks::BookmarkNode*)bookmarkNode;

// Handles the bookkeeping for when a node is created.
- (void)didCreateBookmarkNode:(const bookmarks::BookmarkNode*)bookmarkNode;

@end

#endif  // CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_NODE_APPLESCRIPT_H_
