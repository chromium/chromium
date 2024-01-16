// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/applescript/bookmark_node_applescript.h"

#import "base/apple/foundation_util.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/strings/sys_string_conversions.h"
#include "base/uuid.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#import "chrome/browser/chrome_browser_application_mac.h"
#include "chrome/browser/profiles/profile.h"
#import "chrome/browser/ui/cocoa/applescript/bookmark_item_applescript.h"
#import "chrome/browser/ui/cocoa/applescript/error_applescript.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
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
  base::Uuid _bookmarkGUID;
}

@synthesize tempTitle = _tempTitle;

- (instancetype)init {
  if ((self = [super init])) {
    _bookmarkGUID = base::Uuid::GenerateRandomV4();
    self.uniqueID = [NSString
        stringWithFormat:@"%s", _bookmarkGUID.AsLowercaseString().c_str()];
    self.tempTitle = @"";
  }
  return self;
}

- (instancetype)initWithBookmarkNode:(const BookmarkNode*)bookmarkNode {
  if (!bookmarkNode) {
    self = nil;
    return nil;
  }

  if ((self = [super init])) {
    _bookmarkGUID = bookmarkNode->uuid();
    self.uniqueID = [NSString
        stringWithFormat:@"%s", _bookmarkGUID.AsLowercaseString().c_str()];
  }
  return self;
}

- (base::Uuid)bookmarkGUID {
  return _bookmarkGUID;
}

- (void)didCreateBookmarkNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  CHECK(bookmarkNode);
  CHECK_EQ(bookmarkNode->uuid(), _bookmarkGUID);

  self.title = self.tempTitle;
}

- (const bookmarks::BookmarkNode*)bookmarkNode {
  return self.bookmarkModel->GetNodeByUuid(
      _bookmarkGUID,
      bookmarks::BookmarkModel::NodeTypeForUuidLookup::kLocalOrSyncableNodes);
}

- (NSString*)title {
  const BookmarkNode* bookmarkNode = self.bookmarkNode;
  if (!bookmarkNode) {
    return self.tempTitle;
  }

  return base::SysUTF16ToNSString(bookmarkNode->GetTitle());
}

- (void)setTitle:(NSString*)title {
  // If the scripter enters:
  //
  //   make new bookmarks folder with properties {title:"foo"}
  //
  // the node has not yet been created so title is stored in the temp title.
  const BookmarkNode* bookmarkNode = self.bookmarkNode;
  if (!bookmarkNode) {
    self.tempTitle = title;
    return;
  }

  BookmarkModel* model = self.bookmarkModel;
  if (!model) {
    return;
  }

  model->SetTitle(bookmarkNode, base::SysNSStringToUTF16(title),
                  bookmarks::metrics::BookmarkEditSource::kOther);
}

- (NSNumber*)index {
  const BookmarkNode* bookmarkNode = self.bookmarkNode;
  if (!bookmarkNode) {
    return nil;
  }

  const BookmarkNode* parent = bookmarkNode->parent();
  if (!parent) {
    return nil;
  }

  size_t index = parent->GetIndexOf(bookmarkNode).value();
  // NOTE: AppleScript is 1-Based.
  return @(index + 1);
}

- (BookmarkModel*)bookmarkModel {
  Profile* lastProfile = AppController.sharedController.lastProfile;
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
