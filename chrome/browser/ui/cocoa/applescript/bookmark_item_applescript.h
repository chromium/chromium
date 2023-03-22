// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_ITEM_APPLESCRIPT_H_
#define CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_ITEM_APPLESCRIPT_H_

#import <Foundation/Foundation.h>

#import "chrome/browser/ui/cocoa/applescript/bookmark_node_applescript.h"

// Represents a bookmark item scriptable object in applescript.
@interface BookmarkItemAppleScript : BookmarkNodeAppleScript

// Assigns a node, sets its unique ID and also copies temporary values.
- (void)setBookmarkNode:(const bookmarks::BookmarkNode*)aBookmarkNode;

// Returns/sets the URL that the bookmark item holds.
@property(copy) NSString* URL;

@end

#endif  // CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_ITEM_APPLESCRIPT_H_
