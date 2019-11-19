// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/applescript/bookmark_folder_applescript.h"

#import "base/mac/scoped_nsobject.h"
#import "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/applescript/bookmark_item_applescript.h"
#import "chrome/browser/ui/cocoa/applescript/constants_applescript.h"
#include "chrome/browser/ui/cocoa/applescript/error_applescript.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "url/gurl.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

@implementation BookmarkFolderAppleScript

- (NSArray*)bookmarkFolders {
  NSMutableArray* bookmarkFolders =
      [NSMutableArray arrayWithCapacity:bookmarkNode_->children().size()];

  for (const auto& node : bookmarkNode_->children()) {
    if (!node->is_folder())
      continue;
    base::scoped_nsobject<BookmarkFolderAppleScript> bookmarkFolder(
        [[BookmarkFolderAppleScript alloc] initWithBookmarkNode:node.get()]);
    [bookmarkFolder setContainer:self
                        property:AppleScript::kBookmarkFoldersProperty];
    [bookmarkFolders addObject:bookmarkFolder];
  }

  return bookmarkFolders;
}

- (void)insertInBookmarkFolders:(id)aBookmarkFolder {
  // This method gets called when a new bookmark folder is created so
  // the container and property are set here.
  [aBookmarkFolder setContainer:self
                       property:AppleScript::kBookmarkFoldersProperty];
  BookmarkModel* model = [self bookmarkModel];
  if (!model)
    return;

  const BookmarkNode* node = model->AddFolder(
      bookmarkNode_, bookmarkNode_->children().size(), base::string16());
  if (!node) {
    AppleScript::SetError(AppleScript::errCreateBookmarkFolder);
    return;
  }

  [aBookmarkFolder setBookmarkNode:node];
}

- (void)insertInBookmarkFolders:(id)aBookmarkFolder atIndex:(size_t)index {
  // This method gets called when a new bookmark folder is created so
  // the container and property are set here.
  [aBookmarkFolder setContainer:self
                       property:AppleScript::kBookmarkFoldersProperty];
  size_t position = [self calculatePositionOfBookmarkFolderAt:index];

  BookmarkModel* model = [self bookmarkModel];
  if (!model)
    return;

  const BookmarkNode* node = model->AddFolder(bookmarkNode_,
                                              position,
                                              base::string16());
  if (!node) {
    AppleScript::SetError(AppleScript::errCreateBookmarkFolder);
    return;
  }

  [aBookmarkFolder setBookmarkNode:node];
}

- (void)removeFromBookmarkFoldersAtIndex:(size_t)index {
  size_t position = [self calculatePositionOfBookmarkFolderAt:index];

  BookmarkModel* model = [self bookmarkModel];
  if (!model)
    return;

  model->Remove(bookmarkNode_->children()[position].get());
}

- (NSArray*)bookmarkItems {
  NSMutableArray* bookmarkItems =
      [NSMutableArray arrayWithCapacity:bookmarkNode_->children().size()];

  for (const auto& node : bookmarkNode_->children()) {
    if (!node->is_url())
      continue;
    base::scoped_nsobject<BookmarkItemAppleScript> bookmarkItem(
        [[BookmarkItemAppleScript alloc] initWithBookmarkNode:node.get()]);
    [bookmarkItem setContainer:self
                      property:AppleScript::kBookmarkItemsProperty];
    [bookmarkItems addObject:bookmarkItem];
  }

  return bookmarkItems;
}

- (void)insertInBookmarkItems:(BookmarkItemAppleScript*)aBookmarkItem {
  // This method gets called when a new bookmark item is created so
  // the container and property are set here.
  [aBookmarkItem setContainer:self
                     property:AppleScript::kBookmarkItemsProperty];

  BookmarkModel* model = [self bookmarkModel];
  if (!model)
    return;

  GURL url = GURL(base::SysNSStringToUTF8([aBookmarkItem URL]));
  if (!url.is_valid()) {
    AppleScript::SetError(AppleScript::errInvalidURL);
    return;
  }

  const BookmarkNode* node = model->AddURL(
      bookmarkNode_, bookmarkNode_->children().size(), base::string16(), url);
  if (!node) {
    AppleScript::SetError(AppleScript::errCreateBookmarkItem);
    return;
  }

  [aBookmarkItem setBookmarkNode:node];
}

- (void)insertInBookmarkItems:(BookmarkItemAppleScript*)aBookmarkItem
                      atIndex:(size_t)index {
  // This method gets called when a new bookmark item is created so
  // the container and property are set here.
  [aBookmarkItem setContainer:self
                     property:AppleScript::kBookmarkItemsProperty];
  size_t position = [self calculatePositionOfBookmarkItemAt:index];

  BookmarkModel* model = [self bookmarkModel];
  if (!model)
    return;

  GURL url(base::SysNSStringToUTF8([aBookmarkItem URL]));
  if (!url.is_valid()) {
    AppleScript::SetError(AppleScript::errInvalidURL);
    return;
  }

  const BookmarkNode* node = model->AddURL(bookmarkNode_,
                                           position,
                                           base::string16(),
                                           url);
  if (!node) {
    AppleScript::SetError(AppleScript::errCreateBookmarkItem);
    return;
  }

  [aBookmarkItem setBookmarkNode:node];
}

- (void)removeFromBookmarkItemsAtIndex:(size_t)index {
  size_t position = [self calculatePositionOfBookmarkItemAt:index];

  BookmarkModel* model = [self bookmarkModel];
  if (!model)
    return;

  model->Remove(bookmarkNode_->children()[position].get());
}

- (size_t)calculatePositionOfBookmarkFolderAt:(size_t)index {
  // Traverse through all the child nodes till the required node is found and
  // return its position.
  // AppleScript is 1-based therefore index is incremented by 1.
  ++index;
  size_t count = 0;
  while (index) {
    if (bookmarkNode_->children()[count++]->is_folder())
      --index;
  }
  return count - 1;
}

- (size_t)calculatePositionOfBookmarkItemAt:(size_t)index {
  // Traverse through all the child nodes till the required node is found and
  // return its position.
  // AppleScript is 1-based therefore index is incremented by 1.
  ++index;
  size_t count = 0;
  while (index) {
    if (bookmarkNode_->children()[count++]->is_url())
      --index;
  }
  return count - 1;
}

@end
