// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/applescript/bookmark_item_applescript.h"

#include "base/apple/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/app_controller_mac.h"
#import "chrome/browser/ui/cocoa/applescript/apple_event_util.h"
#import "chrome/browser/ui/cocoa/applescript/error_applescript.h"
#include "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/common/bookmark_metrics.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

@interface BookmarkItemAppleScript ()

// Contains the temporary URL when a user creates a new item with the URL
// specified like:
//
//   make new bookmarks item with properties {URL:"foo"}
@property(nonatomic, copy) NSString* tempURL;

@end

@implementation BookmarkItemAppleScript

@synthesize tempURL = _tempURL;

- (instancetype)init {
  if ((self = [super init])) {
    self.tempURL = @"";
  }
  return self;
}

- (void)didCreateBookmarkNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  [super didCreateBookmarkNode:bookmarkNode];
  self.URL = self.tempURL;
}

- (NSString*)URL {
  if (!self.bookmarkNode) {
    return _tempURL;
  }

  return base::SysUTF8ToNSString(self.bookmarkNode->url().spec());
}

- (void)setURL:(NSString*)url {
  // If a scripter sets a URL before the node is added, the URL is saved at a
  // temporary location.
  if (!self.bookmarkNode) {
    self.tempURL = url;
    return;
  }

  GURL gurl(base::SysNSStringToUTF8(url));
  if (!gurl.is_valid()) {
    AppleScript::SetError(AppleScript::Error::kInvalidURL);
    return;
  }

  if (!chrome::mac::IsJavaScriptEnabledForProfile(
          AppController.sharedController.lastProfile) &&
      gurl.SchemeIs(url::kJavaScriptScheme)) {
    AppleScript::SetError(AppleScript::Error::kJavaScriptUnsupported);
    return;
  }

  BookmarkModel* model = self.bookmarkModel;
  if (!model) {
    return;
  }

  model->SetURL(self.bookmarkNode, gurl,
                bookmarks::metrics::BookmarkEditSource::kOther);
}

@end
