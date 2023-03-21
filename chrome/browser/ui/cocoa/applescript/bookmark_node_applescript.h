// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_NODE_APPLESCRIPT_H_
#define CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_NODE_APPLESCRIPT_H_

#import <Foundation/Foundation.h>

#import "chrome/browser/ui/cocoa/applescript/element_applescript.h"

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}

// Contains all the elements that are common to both a bookmark folder and
// bookmark item.
@interface BookmarkNodeAppleScript : ElementAppleScript

// Does not actually create a folder/item but just sets its ID, the folder is
// created in insertInBookmarksFolder: in the corresponding bookmarks folder.
- (instancetype)init;

// Does not make a folder/item but instead uses an existing one.
- (instancetype)initWithBookmarkNode:
    (const bookmarks::BookmarkNode*)aBookmarkNode;

// Assigns/gets a node, sets its unique ID and also copies temporary values.
- (void)setBookmarkNode:(const bookmarks::BookmarkNode*)aBookmarkNode;
- (const bookmarks::BookmarkNode*)bookmarkNode;

// Get and Set title.
- (NSString*)title;
- (void)setTitle:(NSString*)aTitle;

// Returns the index with respect to its parent bookmark folder.
- (NSNumber*)index;

// Returns the bookmark model of the browser, returns NULL if there is an error.
- (bookmarks::BookmarkModel*)bookmarkModel;

@end

#endif  // CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_NODE_APPLESCRIPT_H_
