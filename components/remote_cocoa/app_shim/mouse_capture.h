// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_MOUSE_CAPTURE_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_MOUSE_CAPTURE_H_

#import <Cocoa/Cocoa.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/remote_cocoa/app_shim/remote_cocoa_app_shim_export.h"

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

  CocoaMouseCapture(const CocoaMouseCapture&) = delete;
  CocoaMouseCapture& operator=(const CocoaMouseCapture&) = delete;

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

  raw_ptr<CocoaMouseCaptureDelegate> delegate_;  // Weak. Owns this.

  // The active event tap for this capture. Owned by this, but can be cleared
  // out early if another instance of CocoaMouseCapture is created.
  std::unique_ptr<ActiveEventTap> active_handle_;

  base::WeakPtrFactory<CocoaMouseCapture> factory_{this};
};

}  // namespace remote_cocoa

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_MOUSE_CAPTURE_H_
