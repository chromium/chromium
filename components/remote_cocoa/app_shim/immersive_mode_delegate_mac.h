// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_IMMERSIVE_MODE_DELEGATE_MAC_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_IMMERSIVE_MODE_DELEGATE_MAC_H_

#include <AppKit/AppKit.h>

@protocol ImmersiveModeDelegate <NSWindowDelegate>
@property(readonly) NSWindow* originalHostingWindow;
@end

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_IMMERSIVE_MODE_DELEGATE_MAC_H_
