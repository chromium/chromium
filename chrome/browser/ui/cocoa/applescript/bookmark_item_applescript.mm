// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/applescript/bookmark_item_applescript.h"

#include "base/mac/foundation_util.h"
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

- (void)dealloc {
  [_tempURL release];
  [super dealloc];
}

- (void)setBookmarkNode:(const BookmarkNode*)aBookmarkNode {
  super.bookmarkNode = aBookmarkNode;
  self.URL = self.tempURL;
}

- (NSString*)URL {
  if (!self.bookmarkNode) {
    return _tempURL;
  }

  return base::SysUTF8ToNSString(self.bookmarkNode->url().spec());
}

- (void)setURL:(NSString*)aURL {
  GURL url(base::SysNSStringToUTF8(aURL));

  AppController* appDelegate =
      base::mac::ObjCCastStrict<AppController>(NSApp.delegate);
  if (!chrome::mac::IsJavaScriptEnabledForProfile(appDelegate.lastProfile) &&
      url.SchemeIs(url::kJavaScriptScheme)) {
    AppleScript::SetError(AppleScript::Error::kJavaScriptUnsupported);
    return;
  }

  // If a scripter sets a URL before the node is added, the URL is saved at a
  // temporary location.
  if (!self.bookmarkNode) {
    self.tempURL = aURL;
    return;
  }

  BookmarkModel* model = self.bookmarkModel;
  if (!model) {
    return;
  }

  if (!url.is_valid()) {
    AppleScript::SetError(AppleScript::Error::kInvalidURL);
    return;
  }

  model->SetURL(self.bookmarkNode, url,
                bookmarks::metrics::BookmarkEditSource::kOther);
}

@end
