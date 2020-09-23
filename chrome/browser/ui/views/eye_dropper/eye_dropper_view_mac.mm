// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/eye_dropper/eye_dropper_view_mac.h"

#import <Cocoa/Cocoa.h>

#include "chrome/browser/ui/views/eye_dropper/eye_dropper_view.h"
#include "content/public/browser/render_frame_host.h"
#include "skia/ext/skia_utils_mac.h"

EyeDropperViewMac::EyeDropperViewMac(content::EyeDropperListener* listener)
    : listener_(listener) {
  if (!listener_)
    return;
  if (@available(macOS 10.15, *)) {
    color_sampler_.reset([[NSColorSampler alloc] init]);
    [color_sampler_ showSamplerWithSelectionHandler:^(NSColor* selectedColor) {
      if (!selectedColor) {
        listener_->ColorSelectionCanceled();
      } else {
        listener_->ColorSelected(skia::NSSystemColorToSkColor(selectedColor));
      }
    }];
  }
}

EyeDropperViewMac::~EyeDropperViewMac() {}

EyeDropperView::PreEventDispatchHandler::PreEventDispatchHandler(
    EyeDropperView* view)
    : view_(view) {
  // Ensure that this handler is called before color popup handler.
  clickEventTap_ = [NSEvent
      addLocalMonitorForEventsMatchingMask:NSAnyEventMask
                                   handler:^NSEvent*(NSEvent* event) {
                                     NSEventType eventType = [event type];
                                     if (eventType == NSLeftMouseDown ||
                                         eventType == NSRightMouseDown) {
                                       view_->OnColorSelected();
                                       return nil;
                                     }

                                     return event;
                                   }];

  // Needed because the local event monitor doesn't see the click on the
  // menubar.
  NSNotificationCenter* notificationCenter =
      [NSNotificationCenter defaultCenter];
  notificationObserver_ =
      [notificationCenter addObserverForName:NSMenuDidBeginTrackingNotification
                                      object:[NSApp mainMenu]
                                       queue:[NSOperationQueue mainQueue]
                                  usingBlock:^(NSNotification* note) {
                                    view_->OnColorSelected();
                                  }];
}

EyeDropperView::PreEventDispatchHandler::~PreEventDispatchHandler() {
  if (clickEventTap_) {
    [NSEvent removeMonitor:clickEventTap_];
    clickEventTap_ = nil;
  }

  if (notificationObserver_) {
    NSNotificationCenter* notificationCenter =
        [NSNotificationCenter defaultCenter];
    [notificationCenter removeObserver:notificationObserver_];
    notificationObserver_ = nil;
  }
}

void EyeDropperView::PreEventDispatchHandler::OnMouseEvent(
    ui::MouseEvent* event) {
  // The event monitor already provides a handler.
}

void EyeDropperView::MoveViewToFront() {
  // Moves the window to the front of the screen list within the popup level
  // since the eye dropper can be opened from the color picker.
  NSWindow* window = GetWidget()->GetNativeWindow().GetNativeNSWindow();
  [window setLevel:NSPopUpMenuWindowLevel];
  [window makeKeyAndOrderFront:nil];
}

void EyeDropperView::HideCursor() {
  [NSCursor hide];
}

void EyeDropperView::ShowCursor() {
  [NSCursor unhide];
}

gfx::Size EyeDropperView::GetSize() const {
  return gfx::Size(90, 90);
}

float EyeDropperView::GetDiameter() const {
  return 90;
}

std::unique_ptr<content::EyeDropper> ShowEyeDropper(
    content::RenderFrameHost* frame,
    content::EyeDropperListener* listener) {
  if (@available(macOS 10.15, *)) {
    return std::make_unique<EyeDropperViewMac>(listener);
  }
  return std::make_unique<EyeDropperView>(frame, listener);
}
