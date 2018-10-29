// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_MENU_COCOA_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_MENU_COCOA_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "ui/base/window_open_disposition.h"

class BookmarkMenuBridge;

namespace bookmarks {
class BookmarkNode;
}

// Controller (MVC) for the bookmark menu.
// All bookmark menu item commands get directed here.
// Unfortunately there is already a C++ class named BookmarkMenuController.
@interface BookmarkMenuCocoaController : NSObject<NSMenuDelegate>

// Make a relevant tooltip string for node.
+ (NSString*)tooltipForNode:(const bookmarks::BookmarkNode*)node;

- (id)initWithBridge:(BookmarkMenuBridge*)bridge;

// Called by any Bookmark menu item.
// The menu item's tag is the bookmark ID.
- (IBAction)openBookmarkMenuItem:(id)sender;

@end  // BookmarkMenuCocoaController


@interface BookmarkMenuCocoaController (ExposedForUnitTests)
- (const bookmarks::BookmarkNode*)nodeForIdentifier:(int)identifier;
- (void)openURLForNode:(const bookmarks::BookmarkNode*)node;
- (void)openAll:(NSInteger)tag
    withDisposition:(WindowOpenDisposition)disposition;
@end  // BookmarkMenuCocoaController (ExposedForUnitTests)

#endif  // CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_MENU_COCOA_CONTROLLER_H_
