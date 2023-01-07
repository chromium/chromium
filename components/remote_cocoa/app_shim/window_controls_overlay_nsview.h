// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_WINDOW_CONTROLS_OVERLAY_NSVIEW_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_WINDOW_CONTROLS_OVERLAY_NSVIEW_H_

#import <Cocoa/Cocoa.h>

#include "components/remote_cocoa/app_shim/remote_cocoa_app_shim_export.h"
#include "ui/gfx/geometry/rect.h"

namespace remote_cocoa {
class NativeWidgetNSWindowBridge;
}  // namespace remote_cocoa

REMOTE_COCOA_APP_SHIM_EXPORT
@interface WindowControlsOverlayNSView : NSView {
 @private
  // Weak.
  remote_cocoa::NativeWidgetNSWindowBridge* _bridge;
}
@property(readonly, nonatomic) remote_cocoa::NativeWidgetNSWindowBridge* bridge;

- (instancetype)initWithBridge:
    (remote_cocoa::NativeWidgetNSWindowBridge*)bridge;

- (void)updateBounds:(gfx::Rect)bounds;
@end

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_WINDOW_CONTROLS_OVERLAY_NSVIEW_H_
