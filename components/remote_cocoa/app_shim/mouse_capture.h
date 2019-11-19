// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_MOUSE_CAPTURE_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_MOUSE_CAPTURE_H_

#include <memory>

#include "base/macros.h"
#include "components/remote_cocoa/app_shim/remote_cocoa_app_shim_export.h"

@class NSWindow;

namespace remote_cocoa {

class CocoaMouseCaptureDelegate;

// Basic mouse capture to simulate ::SetCapture() from Windows. This is used to
// support menu widgets (e.g. on Combo boxes). Clicking anywhere other than the
// menu should dismiss the menu and "swallow" the mouse event. All events are
// forwarded, but only events to the same application are "swallowed", which is
// consistent with how native NSMenus behave.
class REMOTE_COCOA_APP_SHIM_EXPORT CocoaMouseCapture {
 public:
  explicit CocoaMouseCapture(CocoaMouseCaptureDelegate* delegate);
  ~CocoaMouseCapture();

  // Returns the NSWindow with capture or nil if no window has capture
  // currently.
  static NSWindow* GetGlobalCaptureWindow();

  // True if the event tap is active (i.e. not stolen by a later instance).
  bool IsActive() const { return !!active_handle_; }

 private:
  class ActiveEventTap;

  // Deactivates the event tap if still active.
  void OnOtherClientGotCapture();

  CocoaMouseCaptureDelegate* delegate_;  // Weak. Owns this.

  // The active event tap for this capture. Owned by this, but can be cleared
  // out early if another instance of CocoaMouseCapture is created.
  std::unique_ptr<ActiveEventTap> active_handle_;

  DISALLOW_COPY_AND_ASSIGN(CocoaMouseCapture);
};

}  // namespace remote_cocoa

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_MOUSE_CAPTURE_H_
