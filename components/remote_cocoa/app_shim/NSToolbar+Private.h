// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_NSTOOLBAR_PRIVATE_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_NSTOOLBAR_PRIVATE_H_

#import <AppKit/AppKit.h>

@interface NSToolbar (Private)
// Returns the view that backs the toolbar or nil.
- (NSView *)privateToolbarView;
@end

#endif // COMPONENTS_REMOTE_COCOA_APP_SHIM_NSTOOLBAR_PRIVATE_H_
