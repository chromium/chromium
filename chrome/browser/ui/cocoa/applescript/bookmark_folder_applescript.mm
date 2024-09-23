// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/applescript/bookmark_folder_applescript.h"

#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/applescript/bookmark_item_applescript.h"
#import "chrome/browser/ui/cocoa/applescript/constants_applescript.h"
#include "chrome/browser/ui/cocoa/applescript/error_applescript.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "url/gurl.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

// /!\ Warning
//
// The design of the AppleScript dictionary with regards to bookmarks is
// deficient. The ideal design would mirror the design of the actual bookmark
// system, where a bookmark element could be either a folder or an item, and
// bookmark folders could hold any number of them, in any order.
//
// However, that's not the design that is implemented. The current design is
// that bookmark folders and bookmark items are separate and unrelated things,
// and that bookmark folders have a list of bookmark folders they contain, as
// well as a list of bookmark items they contain.
//
// That is, there are _two separate lists_.
//
// Translating from the real bookmarks system, where the children of a folder
// are a list of intermingled folders and items, is easy: walk the list of
// children, and filter out the wrong kind. (See `-bookmarkFolders` and
// `-bookmarkItems`.)
//
// Translating _to_ the real bookmarks system cannot be done in the general
// case. There is no control over the mingling of folders and items, and there
// is only vague control over the ordering of items that share a type.
//
// All this is to explain the difference in terminology between "index" and
// "bookmark manager position". An "index" value is relative to the two separate
// child lists that exist from the AppleScript perspective. The "bookmark
// manager position" is relative to the true list of (both types of) children of
// a folder in the bookmarks system.
//
// Because AppleScript maintains no state, no translation ever needs to be done
// from positions to indexes. AppleScript keeps object identifiers, and if it
// ever needs to update the index, it will re-request the list and find the item
// again by matching its ID.
//
// However, when a script requests a folder or bookmark to be moved around or
// inserted, AppleScript will request a change in index. That index will need to
// be converted into a position in the real list of children of a folder, and
// that's what the `-bookmarkManagerPositionOf[Folder|Item]At:` methods are for.

@interface BookmarkFolderAppleScript ()

// Does the actual insertion of a bookmark folder at an absolute position.
- (void)insertFolder:(BookmarkFolderAppleScript*)bookmarkFolder
    atBookmarkManagerPosition:(size_t)position;

// Does the actual insertion of a bookmark item at an absolute position.
- (void)insertItem:(BookmarkItemAppleScript*)bookmarkItem
    atBookmarkManagerPosition:(size_t)position;

// Returns the position of a bookmark folder within the current bookmark folder
// which consists of bookmark folders as well as bookmark items.
- (size_t)bookmarkManagerPositionOfFolderAt:(size_t)index;

// Returns the position of a bookmark item within the current bookmark folder
// which consists of bookmark folders as well as bookmark items.
- (size_t)bookmarkManagerPositionOfItemAt:(size_t)index;

@end

@implementation BookmarkFolderAppleScript

- (NSArray<BookmarkFolderAppleScript*>*)bookmarkFolders {
  NSMutableArray<BookmarkFolderAppleScript*>* bookmarkFolders =
      [NSMutableArray arrayWithCapacity:self.bookmarkNode->children().size()];

  for (const auto& node : self.bookmarkNode->children()) {
    if (!node->is_folder()) {
      continue;
    }

    BookmarkFolderAppleScript* bookmarkFolder =
        [[BookmarkFolderAppleScript alloc] initWithBookmarkNode:node.get()];
    [bookmarkFolder setContainer:self
                        property:AppleScript::kBookmarkFoldersProperty];
    [bookmarkFolders addObject:bookmarkFolder];
  }

  return bookmarkFolders;
}

- (NSArray<BookmarkItemAppleScript*>*)bookmarkItems {
  NSMutableArray<BookmarkItemAppleScript*>* bookmarkItems =
      [NSMutableArray arrayWithCapacity:self.bookmarkNode->children().size()];

  for (const auto& node : self.bookmarkNode->children()) {
    if (!node->is_url()) {
      continue;
    }

    BookmarkItemAppleScript* bookmarkItem =
        [[BookmarkItemAppleScript alloc] initWithBookmarkNode:node.get()];
    [bookmarkItem setContainer:self
                      property:AppleScript::kBookmarkItemsProperty];
    [bookmarkItems addObject:bookmarkItem];
  }

  return bookmarkItems;
}

- (void)insertInBookmarkFolders:(BookmarkFolderAppleScript*)bookmarkFolder {
  [self insertFolder:bookmarkFolder
      atBookmarkManagerPosition:self.bookmarkNode->children().size()];
}

- (void)insertInBookmarkFolders:(BookmarkFolderAppleScript*)bookmarkFolder
                        atIndex:(size_t)index {
  [self insertFolder:bookmarkFolder
      atBookmarkManagerPosition:[self bookmarkManagerPositionOfFolderAt:index]];
}

- (void)insertFolder:(BookmarkFolderAppleScript*)bookmarkFolder
    atBookmarkManagerPosition:(size_t)position {
  [bookmarkFolder setContainer:self
                      property:AppleScript::kBookmarkFoldersProperty];

  BookmarkModel* model = self.bookmarkModel;
  if (!model) {
    return;
  }

  const BookmarkNode* node = model->AddFolder(
      self.bookmarkNode, position,
      /*title=*/std::u16string(), /*meta_info=*/nullptr,
      /*creation_time=*/std::nullopt, bookmarkFolder.bookmarkGUID);
  if (!node) {
    AppleScript::SetError(AppleScript::Error::kCreateBookmarkFolder);
    return;
  }

  [bookmarkFolder didCreateBookmarkNode:node];
}

- (void)removeFromBookmarkFoldersAtIndex:(size_t)index {
  size_t position = [self bookmarkManagerPositionOfFolderAt:index];

  BookmarkModel* model = self.bookmarkModel;
  if (!model) {
    return;
  }

  model->Remove(self.bookmarkNode->children()[position].get(),
                bookmarks::metrics::BookmarkEditSource::kUser, FROM_HERE);
}

- (void)insertInBookmarkItems:(BookmarkItemAppleScript*)bookmarkItem {
  [self insertItem:bookmarkItem
      atBookmarkManagerPosition:self.bookmarkNode->children().size()];
}

- (void)insertInBookmarkItems:(BookmarkItemAppleScript*)bookmarkItem
                      atIndex:(size_t)index {
  [self insertItem:bookmarkItem
      atBookmarkManagerPosition:[self bookmarkManagerPositionOfItemAt:index]];
}

- (void)insertItem:(BookmarkItemAppleScript*)bookmarkItem
    atBookmarkManagerPosition:(size_t)position {
  [bookmarkItem setContainer:self property:AppleScript::kBookmarkItemsProperty];

  BookmarkModel* model = self.bookmarkModel;
  if (!model) {
    return;
  }

  GURL url(base::SysNSStringToUTF8(bookmarkItem.URL));
  if (!url.is_valid()) {
    AppleScript::SetError(AppleScript::Error::kInvalidURL);
    return;
  }

  const BookmarkNode* node = model->AddURL(
      self.bookmarkNode, position, /*title=*/std::u16string(), url,
      /*meta_info=*/nullptr, /*creation_time=*/std::nullopt,
      bookmarkItem.bookmarkGUID, /*added_by_user=*/true);
  if (!node) {
    AppleScript::SetError(AppleScript::Error::kCreateBookmarkItem);
    return;
  }

  [bookmarkItem didCreateBookmarkNode:node];
}

- (void)removeFromBookmarkItemsAtIndex:(size_t)index {
  size_t position = [self bookmarkManagerPositionOfItemAt:index];

  BookmarkModel* model = self.bookmarkModel;
  if (!model) {
    return;
  }

  model->Remove(self.bookmarkNode->children()[position].get(),
                bookmarks::metrics::BookmarkEditSource::kUser, FROM_HERE);
}

- (size_t)bookmarkManagerPositionOfFolderAt:(size_t)index {
  // Traverse through all the child nodes until the required node is found and
  // return its position.
  // AppleScript is 1-based therefore index is incremented by 1.
  ++index;
  size_t count = 0;
  while (index) {
    if (self.bookmarkNode->children()[count++]->is_folder()) {
      --index;
    }
  }
  return count - 1;
}

- (size_t)bookmarkManagerPositionOfItemAt:(size_t)index {
  // Traverse through all the child nodes until the required node is found and
  // return its position.
  // AppleScript is 1-based therefore index is incremented by 1.
  ++index;
  size_t count = 0;
  while (index) {
    if (self.bookmarkNode->children()[count++]->is_url()) {
      --index;
    }
  }
  return count - 1;
}

@end
