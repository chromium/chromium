// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/browser_help_bubble_event_relay.h"

#include <memory>

#include "components/remote_cocoa/app_shim/mouse_capture.h"
#include "components/remote_cocoa/app_shim/mouse_capture_delegate.h"
#include "ui/events/event.h"
#include "ui/events/event_target.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/mac/coordinate_conversion.h"
#include "ui/gfx/native_widget_types.h"

class WindowHelpBubbleEventRelayMac::Delegate
    : public remote_cocoa::CocoaMouseCaptureDelegate {
 public:
  Delegate(views::Widget* primary_widget, WindowHelpBubbleEventRelayMac& owner)
      : observed_widget_(primary_widget), owner_(owner) {}
  ~Delegate() override = default;

  bool PostCapturedEvent(NSEvent* event) override {
    std::unique_ptr<ui::Event> ui_event =
        ui::EventFromNative(base::apple::OwnedNSEvent(event));
    if (!ui_event || !ui_event->IsLocatedEvent()) {
      return false;
    }

    NSPoint screen_point = event.locationInWindow;
    // If there's no window, the event is in screen coordinates.
    if (event.window) {
      NSRect frame = [event.window contentRectForFrameRect:event.window.frame];
      screen_point.x += frame.origin.x;
      screen_point.y += frame.origin.y;
    }
    const gfx::Point screen_coords = gfx::ScreenPointFromNSPoint(screen_point);

    ui::LocatedEvent* const located_event = ui_event->AsLocatedEvent();
    return owner_->OnEvent(*located_event, screen_coords);
  }

  void OnMouseCaptureLost() override { owner_->OnConnectionLost(); }

  NSWindow* GetWindow() const override {
    // This isn't used for this use case and might be null anyway due to in/out
    // of process stuff. Safer to just return null.
    return nullptr;
  }

 private:
  const raw_ptr<views::Widget> observed_widget_;
  const raw_ref<WindowHelpBubbleEventRelayMac> owner_;
  remote_cocoa::CocoaMouseCapture capture_{this};
};

WindowHelpBubbleEventRelayMac::WindowHelpBubbleEventRelayMac(
    views::Widget* primary_widget)
    : WindowHelpBubbleEventRelay(primary_widget),
      delegate_(std::make_unique<Delegate>(primary_widget, *this)) {}

WindowHelpBubbleEventRelayMac::~WindowHelpBubbleEventRelayMac() {
  Release();
}

void WindowHelpBubbleEventRelayMac::Release() {
  delegate_.reset();
  WindowHelpBubbleEventRelay::Release();
}
