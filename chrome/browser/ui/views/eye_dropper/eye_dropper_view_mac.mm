// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/eye_dropper/eye_dropper_view_mac.h"

#import <Cocoa/Cocoa.h>

// Include Carbon to use the keycode names in Carbon's Event.h
#include <Carbon/Carbon.h>

#include "chrome/browser/ui/views/eye_dropper/eye_dropper_view.h"
#include "content/public/browser/render_frame_host.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/ui_base_features.h"

EyeDropperViewMac::EyeDropperViewMac(content::EyeDropperListener* listener)
    : listener_(listener), weak_ptr_factory_(this) {
  if (!listener_)
    return;
  if (@available(macOS 10.15, *)) {
    color_sampler_.reset([[NSColorSampler alloc] init]);
    // Used to ensure that EyeDropperViewMac is still alive when the handler is
    // called.
    base::WeakPtr<EyeDropperViewMac> weak_this = weak_ptr_factory_.GetWeakPtr();
    [color_sampler_ showSamplerWithSelectionHandler:^(NSColor* selectedColor) {
      if (!weak_this)
        return;
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
    EyeDropperView* view,
    gfx::NativeView parent)
    : view_(view) {
  // Ensure that this handler is called before color popup handler.
  clickEventTap_ = [NSEvent
      addLocalMonitorForEventsMatchingMask:NSEventMaskAny
                                   handler:^NSEvent*(NSEvent* event) {
                                     NSEventType eventType = [event type];
                                     if (eventType ==
                                             NSEventTypeLeftMouseDown ||
                                         eventType ==
                                             NSEventTypeRightMouseDown) {
                                       view_->OnColorSelected();
                                       return nil;
                                     } else if (eventType ==
                                                    NSEventTypeKeyDown &&
                                                [event keyCode] == kVK_Escape) {
                                       view_->OnColorSelectionCanceled();
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

void EyeDropperView::CaptureInputIfNeeded() {
  // There is no need to capture input on Mac.
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
  if (!features::IsEyeDropperEnabled()) {
    return nullptr;
  }

  if (@available(macOS 10.15, *)) {
    return std::make_unique<EyeDropperViewMac>(listener);
  }
  return std::make_unique<EyeDropperView>(frame, listener);
}
