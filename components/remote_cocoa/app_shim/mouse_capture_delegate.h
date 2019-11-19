// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_MOUSE_CAPTURE_DELEGATE_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_MOUSE_CAPTURE_DELEGATE_H_

@class NSEvent;
@class NSWindow;

namespace remote_cocoa {

// Delegate for receiving captured events from a CocoaMouseCapture.
class CocoaMouseCaptureDelegate {
 public:
  // Called when an event has been captured. This may be an event local to the
  // application, or a global event (sent to another application). If it is a
  // local event, regular event handling will be suppressed.
  virtual void PostCapturedEvent(NSEvent* event) = 0;

  // Called once. When another window acquires capture, or when the
  // CocoaMouseCapture is destroyed.
  virtual void OnMouseCaptureLost() = 0;

  // Returns the associated NSWindow.
  virtual NSWindow* GetWindow() const = 0;
};

}  // namespace remote_cocoa

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_MOUSE_CAPTURE_DELEGATE_H_
