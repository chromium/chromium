// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/remote_cocoa/app_shim/mouse_capture.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "base/macros.h"
#import "components/remote_cocoa/app_shim/mouse_capture_delegate.h"
#include "ui/base/cocoa/weak_ptr_nsobject.h"

namespace remote_cocoa {

// The ActiveEventTap is a RAII handle on the resources being used to capture
// events. There is either 0 or 1 active instance of this class. If a second
// instance is created, it will destroy the other instance before returning from
// its constructor.
class CocoaMouseCapture::ActiveEventTap {
 public:
  explicit ActiveEventTap(CocoaMouseCapture* owner);
  ~ActiveEventTap();

  // Returns the NSWindow with capture or nil if no window has capture
  // currently.
  static NSWindow* GetGlobalCaptureWindow();

  void Init();

 private:
  // Returns the associated NSWindow with capture.
  NSWindow* GetCaptureWindow() const;

  // The currently active event tap, or null if there is none.
  static ActiveEventTap* g_active_event_tap;

  CocoaMouseCapture* owner_;  // Weak. Owns this.
  id local_monitor_;
  id global_monitor_;
  ui::WeakPtrNSObjectFactory<CocoaMouseCapture> factory_;

  DISALLOW_COPY_AND_ASSIGN(ActiveEventTap);
};

CocoaMouseCapture::ActiveEventTap*
    CocoaMouseCapture::ActiveEventTap::g_active_event_tap = nullptr;

CocoaMouseCapture::ActiveEventTap::ActiveEventTap(CocoaMouseCapture* owner)
    : owner_(owner),
      local_monitor_(nil),
      global_monitor_(nil),
      factory_(owner) {
  if (g_active_event_tap)
    g_active_event_tap->owner_->OnOtherClientGotCapture();
  DCHECK(!g_active_event_tap);
  g_active_event_tap = this;
}

CocoaMouseCapture::ActiveEventTap::~ActiveEventTap() {
  DCHECK_EQ(g_active_event_tap, this);
  [NSEvent removeMonitor:global_monitor_];
  [NSEvent removeMonitor:local_monitor_];
  g_active_event_tap = nullptr;
  owner_->delegate_->OnMouseCaptureLost();
}

// static
NSWindow* CocoaMouseCapture::ActiveEventTap::GetGlobalCaptureWindow() {
  return g_active_event_tap ? g_active_event_tap->GetCaptureWindow() : nil;
}

void CocoaMouseCapture::ActiveEventTap::Init() {
  // Consume most things, but not NSMouseEntered/Exited: The Widget doing
  // capture will still see its own Entered/Exit events, but not those for other
  // NSViews, since consuming those would break their tracking area logic.
  NSEventMask event_mask =
      NSLeftMouseDownMask | NSLeftMouseUpMask | NSRightMouseDownMask |
      NSRightMouseUpMask | NSMouseMovedMask | NSLeftMouseDraggedMask |
      NSRightMouseDraggedMask | NSScrollWheelMask | NSOtherMouseDownMask |
      NSOtherMouseUpMask | NSOtherMouseDraggedMask;

  // Capture a WeakPtr via NSObject. This allows the block to detect another
  // event monitor for the same event deleting |owner_|.
  WeakPtrNSObject* handle = factory_.handle();

  auto local_block = ^NSEvent*(NSEvent* event) {
    CocoaMouseCapture* owner =
        ui::WeakPtrNSObjectFactory<CocoaMouseCapture>::Get(handle);
    if (owner)
      owner->delegate_->PostCapturedEvent(event);
    return nil;  // Swallow all local events.
  };
  auto global_block = ^void(NSEvent* event) {
    CocoaMouseCapture* owner =
        ui::WeakPtrNSObjectFactory<CocoaMouseCapture>::Get(handle);
    if (owner)
      owner->delegate_->PostCapturedEvent(event);
  };
  local_monitor_ = [NSEvent addLocalMonitorForEventsMatchingMask:event_mask
                                                         handler:local_block];
  global_monitor_ =
      [NSEvent addGlobalMonitorForEventsMatchingMask:event_mask
                                             handler:global_block];
}

NSWindow* CocoaMouseCapture::ActiveEventTap::GetCaptureWindow() const {
  return owner_->delegate_->GetWindow();
}

CocoaMouseCapture::CocoaMouseCapture(CocoaMouseCaptureDelegate* delegate)
    : delegate_(delegate), active_handle_(new ActiveEventTap(this)) {
  active_handle_->Init();
}

CocoaMouseCapture::~CocoaMouseCapture() {}

// static
NSWindow* CocoaMouseCapture::GetGlobalCaptureWindow() {
  return ActiveEventTap::GetGlobalCaptureWindow();
}

void CocoaMouseCapture::OnOtherClientGotCapture() {
  DCHECK(active_handle_);
  active_handle_.reset();
}

}  // namespace remote_cocoa
