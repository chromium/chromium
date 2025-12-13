// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_OVERRIDE_NS_NEXT_STEP_FRAME_HIT_TEST_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_OVERRIDE_NS_NEXT_STEP_FRAME_HIT_TEST_H_

#include "ui/gfx/native_ui_types.h"

namespace remote_cocoa {

// Overrides an internal AppKit method, -[NSNextStepFrame _hitTestForEvent:],
// to work around an issue introduced in macOS 26.
// In macOS 26, right-clicking on the tabstrip in fullscreen mode no longer
// delivers the mouse-down event to Chrome's view, which prevents Chrome from
// opening the context menus.
//
// This overriding intercepts the hit-testing logic for right-mouse-down events.
// If the event occurs within the bounds of a specific target view (set by
// SetNSNextStepFrameHitTestTargetView), it returns that view as the target.
// For all other events, it calls the original AppKit implementation.
void OverrideNSNextStepFrameHitTest();

// Sets the target NSView for the -[NSNextStepFrame _hitTestForEvent:] swizzle.
//
// When a right-mouse-down event occurs within the bounds of `ns_view`, the
// overridden method will return `ns_view`. This should be the view that is
// intended to receive right-clicks (i.e. the BridgedContentView).
void SetNSNextStepFrameHitTestTargetView(NSWindow* fullscreen_window,
                                         NSView* target_view);

}  // namespace remote_cocoa

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_OVERRIDE_NS_NEXT_STEP_FRAME_HIT_TEST_H_
