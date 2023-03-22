// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/applescript/bookmark_node_applescript.h"

#include "base/check.h"
#import "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#import "chrome/browser/chrome_browser_application_mac.h"
#include "chrome/browser/profiles/profile.h"
#import "chrome/browser/ui/cocoa/applescript/bookmark_item_applescript.h"
#import "chrome/browser/ui/cocoa/applescript/error_applescript.h"
#include "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/common/bookmark_metrics.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

@interface BookmarkNodeAppleScript ()

// Contains the temporary title when a user creates a new item with the title
// specified like:
//
//   make new bookmark folder with properties {title:"foo"}
@property (nonatomic, copy) NSString* tempTitle;

@end

@implementation BookmarkNodeAppleScript {
  raw_ptr<const bookmarks::BookmarkNode> _bookmarkNode;  // weak.
}

@synthesize tempTitle = _tempTitle;

- (instancetype)init {
  if ((self = [super init])) {
    BookmarkModel* model = self.bookmarkModel;
    if (!model) {
      [self release];
      return nil;
    }

    self.uniqueID = [NSString stringWithFormat:@"%lld", model->next_node_id()];
    self.tempTitle = @"";
  }
  return self;
}

- (void)dealloc {
  [_tempTitle release];
  [super dealloc];
}

- (instancetype)initWithBookmarkNode:(const BookmarkNode*)aBookmarkNode {
  if (!aBookmarkNode) {
    [self release];
    return nil;
  }

  if ((self = [super init])) {
    // It is safe to be weak, if a bookmark item/folder goes away
    // (eg user deleting a folder) the AppleScript runtime calls
    // bookmarkFolders/bookmarkItems in BookmarkFolderAppleScript
    // and this particular bookmark item/folder is never returned.
    _bookmarkNode = aBookmarkNode;

    self.uniqueID = [NSString stringWithFormat:@"%lld", aBookmarkNode->id()];
  }
  return self;
}

- (void)setBookmarkNode:(const BookmarkNode*)aBookmarkNode {
  DCHECK(aBookmarkNode);
  // It is safe to be weak, if a bookmark item/folder goes away
  // (eg user deleting a folder) the AppleScript runtime calls
  // bookmarkFolders/bookmarkItems in BookmarkFolderAppleScript
  // and this particular bookmark item/folder is never returned.
  _bookmarkNode = aBookmarkNode;

  self.uniqueID = [NSString stringWithFormat:@"%lld", aBookmarkNode->id()];

  [self setTitle:[self tempTitle]];
}

- (const bookmarks::BookmarkNode*)bookmarkNode {
  return _bookmarkNode;
}

- (NSString*)title {
  if (!_bookmarkNode) {
    return _tempTitle;
  }

  return base::SysUTF16ToNSString(_bookmarkNode->GetTitle());
}

- (void)setTitle:(NSString*)aTitle {
  // If the scripter enters:
  //
  //   make new bookmarks folder with properties {title:"foo"}
  //
  // the node has not yet been created so title is stored in the temp title.
  if (!_bookmarkNode) {
    self.tempTitle = aTitle;
    return;
  }

  BookmarkModel* model = self.bookmarkModel;
  if (!model) {
    return;
  }

  model->SetTitle(_bookmarkNode, base::SysNSStringToUTF16(aTitle),
                  bookmarks::metrics::BookmarkEditSource::kOther);
}

- (NSNumber*)index {
  const BookmarkNode* parent = _bookmarkNode->parent();
  size_t index = parent->GetIndexOf(_bookmarkNode).value();
  // NOTE: AppleScript is 1-Based.
  return @(index + 1);
}

- (BookmarkModel*)bookmarkModel {
  AppController* appDelegate =
      base::mac::ObjCCastStrict<AppController>([NSApp delegate]);

  Profile* lastProfile = appDelegate.lastProfile;
  if (!lastProfile) {
    AppleScript::SetError(AppleScript::Error::kGetProfile);
    return nullptr;
  }

  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(lastProfile);
  if (!model->loaded()) {
    AppleScript::SetError(AppleScript::Error::kBookmarkModelLoad);
    return nullptr;
  }

  return model;
}

@end
