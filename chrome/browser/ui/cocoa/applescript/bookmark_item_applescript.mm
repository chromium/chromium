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

@interface BookmarkItemAppleScript()
@property (nonatomic, copy) NSString* tempURL;
@end

@implementation BookmarkItemAppleScript

@synthesize tempURL = _tempURL;

- (instancetype)init {
  if ((self = [super init])) {
    [self setTempURL:@""];
  }
  return self;
}

- (void)dealloc {
  [_tempURL release];
  [super dealloc];
}

- (void)setBookmarkNode:(const BookmarkNode*)aBookmarkNode {
  [super setBookmarkNode:aBookmarkNode];
  [self setURL:[self tempURL]];
}

- (NSString*)URL {
  if (!_bookmarkNode)
    return _tempURL;

  return base::SysUTF8ToNSString(_bookmarkNode->url().spec());
}

- (void)setURL:(NSString*)aURL {
  GURL url(base::SysNSStringToUTF8(aURL));

  AppController* appDelegate =
      base::mac::ObjCCastStrict<AppController>([NSApp delegate]);
  if (!chrome::mac::IsJavaScriptEnabledForProfile([appDelegate lastProfile]) &&
      url.SchemeIs(url::kJavaScriptScheme)) {
    AppleScript::SetError(AppleScript::errJavaScriptUnsupported);
    return;
  }

  // If a scripter sets a URL before the node is added, URL is saved at a
  // temporary location.
  if (!_bookmarkNode) {
    [self setTempURL:aURL];
    return;
  }

  BookmarkModel* model = [self bookmarkModel];
  if (!model)
    return;

  if (!url.is_valid()) {
    AppleScript::SetError(AppleScript::errInvalidURL);
    return;
  }

  model->SetURL(_bookmarkNode, url,
                bookmarks::metrics::BookmarkEditSource::kOther);
}

@end
