// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_MOUSE_CAPTURE_DELEGATE_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_MOUSE_CAPTURE_DELEGATE_H_

#import <Cocoa/Cocoa.h>

namespace remote_cocoa {

// Delegate for receiving captured events from a CocoaMouseCapture.
class CocoaMouseCaptureDelegate {
 public:
  virtual ~CocoaMouseCaptureDelegate() = default;

  // Called when an event has been captured. This may be an event local to the
  // application, or a global event (sent to another application). If it is a
  // local event and this function returns true, the event will be swallowed
  // instead of propagated normally. The function return value is ignored for
  // global events.
  virtual bool PostCapturedEvent(NSEvent* event) = 0;

  // Called once. When another window acquires capture, or when the
  // CocoaMouseCapture is destroyed.
  virtual void OnMouseCaptureLost() = 0;

  // Returns the associated NSWindow.
  virtual NSWindow* GetWindow() const = 0;
};

}  // namespace remote_cocoa

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_MOUSE_CAPTURE_DELEGATE_H_
