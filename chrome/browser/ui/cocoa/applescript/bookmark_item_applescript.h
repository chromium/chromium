// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_ITEM_APPLESCRIPT_H_
#define CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_ITEM_APPLESCRIPT_H_

#import <Foundation/Foundation.h>

#import "chrome/browser/ui/cocoa/applescript/bookmark_node_applescript.h"

// Represents a bookmark item scriptable object in applescript.
@interface BookmarkItemAppleScript : BookmarkNodeAppleScript

// Returns/sets the URL that the bookmark item holds.
@property(copy) NSString* URL;

// Handles the bookkeeping for when a node is created.
- (void)didCreateBookmarkNode:(const bookmarks::BookmarkNode*)bookmarkNode;

@end

#endif  // CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_ITEM_APPLESCRIPT_H_
