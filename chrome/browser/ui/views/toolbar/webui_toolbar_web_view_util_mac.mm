// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view_util_mac.h"

#include "base/containers/adapters.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/mac/coordinate_conversion.h"
#include "ui/views/cocoa/native_widget_mac_ns_window_host.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/event_monitor.h"
#include "ui/views/widget/widget.h"

WebUIToolbarPressMonitor::WebUIToolbarPressMonitor(BrowserWindowInterface* bwi,
                                                   views::WebView& web_view) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(bwi);
  if (browser_view && browser_view->IsFullscreen()) {
    // With Mac fullscreen, events go to an AppKit window, so we can't
    // really filter by a gfx::NativeWindow well.
    monitor_ = views::EventMonitor::CreateApplicationMonitor(
        this, gfx::NativeWindow(), {ui::EventType::kMousePressed});
  } else {
    monitor_ = views::EventMonitor::CreateWindowMonitor(
        this, web_view.GetWebContents()->GetTopLevelNativeWindow(),
        {ui::EventType::kMousePressed});
  }
}

WebUIToolbarPressMonitor::~WebUIToolbarPressMonitor() = default;

void WebUIToolbarPressMonitor::OnEvent(const ui::Event& event) {
  if (event.type() != ui::EventType::kMousePressed || !event.HasNativeEvent()) {
    return;
  }

  // If we're using the application monitor, we get events from
  // multiple windows so we need to go back to the underlying native
  // event to figure out its coordinate space.
  NSEvent* mac_event = event.native_event().Get();
  NSPoint mac_location = mac_event.locationInWindow;
  // If there's no window, the event is already in screen coordinates.
  NSWindow* window = mac_event.window;
  if (window) {
    mac_location = [window convertPointToScreen:mac_location];
  }
  auto location = gfx::ScreenPointFromNSPoint(mac_location);

  auto adjusted_event = CloneMouseEvent(event.AsMouseEvent());
  adjusted_event->set_location(location);
  adjusted_events_.push_back(std::move(adjusted_event));
  // We need at most 2 events --- previous press, and perhaps duplicate of
  // current one.
  if (adjusted_events_.size() > 2) {
    adjusted_events_.pop_front();
  }
}

ui::MouseEvent* WebUIToolbarPressMonitor::LastAdjustedDisregarding(
    const ui::Event& to_disregard) {
  for (const auto& event : base::Reversed(adjusted_events_)) {
    if (event->time_stamp() == to_disregard.time_stamp()) {
      continue;
    }
    return event.get();
  }
  return nullptr;
}

// static
std::unique_ptr<ui::MouseEvent> WebUIToolbarPressMonitor::CloneMouseEvent(
    const ui::MouseEvent* mouse_event) {
  return base::WrapUnique(mouse_event->Clone().release()->AsMouseEvent());
}
