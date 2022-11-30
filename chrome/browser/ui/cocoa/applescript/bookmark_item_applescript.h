// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_ITEM_APPLESCRIPT_H_
#define CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_ITEM_APPLESCRIPT_H_

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/applescript/bookmark_node_applescript.h"

// Represents a bookmark item scriptable object in applescript.
@interface BookmarkItemAppleScript : BookmarkNodeAppleScript {
 @private
  // Contains the temporary title when a user creates a new item with
  // title specified like
  // |make new bookmarks item with properties {title:"foo"}|.
  NSString* _tempURL;
}

// Assigns a node, sets its unique ID and also copies temporary values.
- (void)setBookmarkNode:(const bookmarks::BookmarkNode*)aBookmarkNode;

// Returns the URL that the bookmark item holds.
- (NSString*)URL;

// Sets the URL of the bookmark item, displays error in applescript console
// if URL is invalid.
- (void)setURL:(NSString*)aURL;

@end

#endif  // CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_ITEM_APPLESCRIPT_H_
