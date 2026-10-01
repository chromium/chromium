// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/webui_location_bar_interactive_uitest_mac.h"

#import <AppKit/AppKit.h>

@interface NSWindow (WebUILocationBarInteractiveUiTestPrivate)
- (void)_setFirstResponder:(NSResponder*)responder;
@end

namespace webui_location_bar_test {

bool SetWindowFirstResponderWithoutResigning(
    gfx::NativeView new_first_responder) {
  NSView* view = new_first_responder.GetNativeNSView();
  NSWindow* window = view.window;
  if (!window || ![window respondsToSelector:@selector(_setFirstResponder:)]) {
    return false;
  }
  [window _setFirstResponder:view];
  return window.firstResponder == view;
}

}  // namespace webui_location_bar_test
