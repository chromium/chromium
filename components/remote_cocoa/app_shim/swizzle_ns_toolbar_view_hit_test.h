// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_SWIZZLE_NS_TOOLBAR_VIEW_HIT_TEST_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_SWIZZLE_NS_TOOLBAR_VIEW_HIT_TEST_H_

namespace remote_cocoa {

// This function swizzles -[NSToolbarView _hitTestForEvent:] to return nil
// for right-mouse down events in fullscreen.
// Used in macOS 26 to work around the issue that right clicking on the
// horizontal tabstrip does not trigger context menu.
void SwizzleNSToolbarViewHitTest();

}  // namespace remote_cocoa

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_SWIZZLE_NS_TOOLBAR_VIEW_HIT_TEST_H_
