// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_drag_api/desktop_tab_drag_impl/tab_drag_session_input_adapter_mac.h"

#import <Cocoa/Cocoa.h>

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_session_input_adapter.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_window_adapter.h"
#include "components/remote_cocoa/app_shim/mouse_capture.h"
#include "components/remote_cocoa/app_shim/mouse_capture_delegate.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/mac/coordinate_conversion.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/event_monitor.h"

namespace tabs_api {

class MacInputAdapter::CocoaMouseCaptureDelegateImpl
    : public remote_cocoa::CocoaMouseCaptureDelegate {
 public:
  explicit CocoaMouseCaptureDelegateImpl(MacInputAdapter* adapter)
      : adapter_(adapter) {}

  bool PostCapturedEvent(NSEvent* event) override {
    return adapter_->PostCapturedEvent(event);
  }
  void OnMouseCaptureLost() override { adapter_->OnMouseCaptureLost(); }
  NSWindow* GetWindow() const override { return adapter_->GetWindow(); }

 private:
  raw_ptr<MacInputAdapter> adapter_;
};

MacInputAdapter::MacInputAdapter()
    : capture_delegate_(std::make_unique<CocoaMouseCaptureDelegateImpl>(this)) {
}

MacInputAdapter::~MacInputAdapter() {
  ReleaseInputCapture();
}

base::expected<void, mojo_base::mojom::ErrorPtr>
MacInputAdapter::StartInputCapture(EventCallback callback,
                                   TabDragWindowAdapter* initial_window) {
  callback_ = std::move(callback);
  active_window_ = initial_window;
  if (GetWindow() != nullptr) {
    capture_ = std::make_unique<remote_cocoa::CocoaMouseCapture>(
        capture_delegate_.get());
  }
  event_monitor_ = views::EventMonitor::CreateApplicationMonitor(
      this, gfx::NativeWindow(), {ui::EventType::kKeyPressed});
  return base::ok();
}

void MacInputAdapter::ReleaseInputCapture() {
  capture_.reset();
  event_monitor_.reset();
  active_window_ = nullptr;
  callback_.Reset();
}

void MacInputAdapter::SuspendInputCapture() {
  suspended_ = true;
  capture_.reset();
}

void MacInputAdapter::ResumeInputCapture() {
  suspended_ = false;
  if (!capture_ && GetWindow() != nullptr) {
    capture_ = std::make_unique<remote_cocoa::CocoaMouseCapture>(
        capture_delegate_.get());
  }
}

void MacInputAdapter::SetActiveWindowContext(TabDragWindowAdapter* new_window) {
  if (active_window_ == new_window && capture_) {
    return;
  }
  const bool was_suspended = suspended_;
  suspended_ = true;
  capture_.reset();
  active_window_ = new_window;
  suspended_ = was_suspended;
  if (!suspended_ && GetWindow() != nullptr) {
    capture_ = std::make_unique<remote_cocoa::CocoaMouseCapture>(
        capture_delegate_.get());
  }
}

bool MacInputAdapter::PostCapturedEvent(NSEvent* event) {
  if (!callback_) {
    return false;
  }
  NSPoint ns_screen_point = [NSEvent mouseLocation];
  if (event.window) {
    ns_screen_point =
        [event.window convertPointToScreen:event.locationInWindow];
  }
  const gfx::Point screen_coords = gfx::ScreenPointFromNSPoint(ns_screen_point);

  if (event.type == NSEventTypeLeftMouseDragged ||
      event.type == NSEventTypeRightMouseDragged ||
      event.type == NSEventTypeOtherMouseDragged ||
      event.type == NSEventTypeMouseMoved) {
    callback_.Run({TabDragInputEvent::Type::kMoved, screen_coords});
    return true;
  } else if (event.type == NSEventTypeLeftMouseUp ||
             event.type == NSEventTypeRightMouseUp ||
             event.type == NSEventTypeOtherMouseUp) {
    callback_.Run({TabDragInputEvent::Type::kDropped, screen_coords});
    return true;
  }
  return false;
}

void MacInputAdapter::OnMouseCaptureLost() {
  if (callback_ && !suspended_) {
    callback_.Run({TabDragInputEvent::Type::kCaptureChanged});
  }
}

NSWindow* MacInputAdapter::GetWindow() const {
  if (active_window_ && active_window_->GetNativeWindow()) {
    return active_window_->GetNativeWindow().GetNativeNSWindow();
  }
  return nullptr;
}

void MacInputAdapter::OnEvent(const ui::Event& event) {
  if (!callback_) {
    return;
  }
  if (event.type() == ui::EventType::kKeyPressed &&
      event.AsKeyEvent()->key_code() == ui::VKEY_ESCAPE) {
    callback_.Run({TabDragInputEvent::Type::kCancelled});
  }
}

}  // namespace tabs_api
