// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/remote_cocoa/app_shim/NSToolbar+Private.h"

// Access the private view that backs the toolbar.
// TODO(http://crbug.com/40261565): Remove when FB12010731 is fixed in AppKit.
@interface NSToolbar (ToolbarView)
// The current usage of this property is readonly. Mark it as such here so we
// don't invite other usages without a thoughtful change.
@property(readonly) NSView *_toolbarView;
@end

@implementation NSToolbar (Private)
- (NSView *)privateToolbarView {
  return [self respondsToSelector:@selector(_toolbarView)] ? self._toolbarView
                                                           : nil;
}
@end
