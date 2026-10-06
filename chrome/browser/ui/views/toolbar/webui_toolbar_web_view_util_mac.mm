// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view_util_mac.h"

#import <AppKit/AppKit.h>

#include <optional>

#include "base/containers/adapters.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/mac/coordinate_conversion.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/cocoa/native_widget_mac_ns_window_host.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/event_monitor.h"
#include "ui/views/widget/widget.h"

namespace {

std::unique_ptr<ui::MouseEvent> CloneMouseEvent(
    const ui::MouseEvent* mouse_event) {
  return base::WrapUnique(mouse_event->Clone().release()->AsMouseEvent());
}

// Delivers `event`, which AppKit delivered to another window (e.g. the omnibox
// popup), to `view` as if AppKit had delivered it to `view` directly. The
// native event is re-created in `view`'s window, keeping its modifiers and
// click count, unless `click_count` is given. Does nothing if `event` has no
// native event or `view` is not in a window.
void DispatchMouseEventToNativeView(const ui::MouseEvent& event,
                                    std::optional<int> click_count,
                                    NSView* view) {
  if (!event.HasNativeEvent()) {
    return;
  }
  NSEvent* ns_event = event.native_event().Get();
  NSWindow* target_window = view.window;
  NSWindow* source_window = ns_event.window;
  if (!target_window || !source_window) {
    return;
  }
  const NSPoint location_in_screen =
      [source_window convertPointToScreen:ns_event.locationInWindow];
  const NSPoint location_in_window =
      [target_window convertPointFromScreen:location_in_screen];

  // RootView synthesizes mouse entered/exited events by copying the current
  // mouse move, so their native event is the move. Re-create them as native
  // entered/exited events.
  if (event.type() == ui::EventType::kMouseEntered ||
      event.type() == ui::EventType::kMouseExited) {
    const bool entered = event.type() == ui::EventType::kMouseEntered;
    NSEvent* enter_exit_event =
        [NSEvent enterExitEventWithType:entered ? NSEventTypeMouseEntered
                                                : NSEventTypeMouseExited
                               location:location_in_window
                          modifierFlags:ns_event.modifierFlags
                              timestamp:ns_event.timestamp
                           windowNumber:target_window.windowNumber
                                context:nil
                            eventNumber:0
                         trackingNumber:0
                               userData:nil];
    if (entered) {
      [view mouseEntered:enter_exit_event];
    } else {
      [view mouseExited:enter_exit_event];
    }
    return;
  }

  NSEvent* mouse_event =
      [NSEvent mouseEventWithType:ns_event.type
                         location:location_in_window
                    modifierFlags:ns_event.modifierFlags
                        timestamp:ns_event.timestamp
                     windowNumber:target_window.windowNumber
                          context:nil
                      eventNumber:ns_event.eventNumber
                       clickCount:click_count.value_or(ns_event.clickCount)
                         pressure:ns_event.pressure];
  switch (mouse_event.type) {
    case NSEventTypeLeftMouseDown:
      [view mouseDown:mouse_event];
      break;
    case NSEventTypeRightMouseDown:
      [view rightMouseDown:mouse_event];
      break;
    case NSEventTypeOtherMouseDown:
      [view otherMouseDown:mouse_event];
      break;
    case NSEventTypeLeftMouseUp:
      [view mouseUp:mouse_event];
      break;
    case NSEventTypeRightMouseUp:
      [view rightMouseUp:mouse_event];
      break;
    case NSEventTypeOtherMouseUp:
      [view otherMouseUp:mouse_event];
      break;
    case NSEventTypeLeftMouseDragged:
      [view mouseDragged:mouse_event];
      break;
    case NSEventTypeRightMouseDragged:
      [view rightMouseDragged:mouse_event];
      break;
    case NSEventTypeOtherMouseDragged:
      [view otherMouseDragged:mouse_event];
      break;
    case NSEventTypeMouseMoved:
      [view mouseMoved:mouse_event];
      break;
    default:
      break;
  }
}

}  // namespace

// Records recent mouse presses in screen coordinates.
// Must be recreated when the browser window enters or
// leaves fullscreen.
class WebUIToolbarEventForwarder::PressMonitor : public ui::EventObserver {
 public:
  explicit PressMonitor(BrowserWindowInterface* bwi, views::WebView& web_view);
  ~PressMonitor() override;

  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override;

  // Returns the most recent recorded mouse press that's not `to_disregard`.
  // May be nullptr if nothing relevant is recorded.
  ui::MouseEvent* LastAdjustedDisregarding(const ui::Event& to_disregard);

  void ClearLastAdjusted() { adjusted_events_.clear(); }

 private:
  std::unique_ptr<views::EventMonitor> monitor_;

  // Recent mouse presses we got, with locations adjusted to screen coordinates.
  // This has more than one since if we're using an application monitor due to
  // fullscreen, we will have duplicates of things seen by the forwarder.
  std::list<std::unique_ptr<ui::MouseEvent>> adjusted_events_;
};

WebUIToolbarEventForwarder::PressMonitor::PressMonitor(
    BrowserWindowInterface* bwi,
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

WebUIToolbarEventForwarder::PressMonitor::~PressMonitor() = default;

void WebUIToolbarEventForwarder::PressMonitor::OnEvent(const ui::Event& event) {
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

ui::MouseEvent*
WebUIToolbarEventForwarder::PressMonitor::LastAdjustedDisregarding(
    const ui::Event& to_disregard) {
  for (const auto& event : base::Reversed(adjusted_events_)) {
    if (event->time_stamp() == to_disregard.time_stamp()) {
      continue;
    }
    return event.get();
  }
  return nullptr;
}

WebUIToolbarEventForwarder::WebUIToolbarEventForwarder(
    WebUIToolbarControlDelegate& control_delegate,
    views::WebView& web_view)
    : control_delegate_(control_delegate), web_view_(web_view) {}

WebUIToolbarEventForwarder::~WebUIToolbarEventForwarder() = default;

void WebUIToolbarEventForwarder::OnMouseEvent(ui::MouseEvent* event) {
  if (!HaveOpenOmniboxPopup()) {
    return;
  }
  if (event->type() == ui::EventType::kMousewheel) {
    // We purposefully don't forward wheel events. They need special phase
    // handling and it doesn't seem like we actually do anything with them.
    return;
  }
  std::optional<int> adjusted_click_count;

  // We only adjust mouse press events since blink generally doesn't pay
  // attention to click counts on releases (copying them from press);
  // except in some scenarios involving pointer lock --- see
  // `blink::MouseEventManager::SetClickCount`.
  if (event->type() == ui::EventType::kMousePressed) {
    // Convert us to screen coordinates so that we can figure out if we're
    // continuing a click sequence started without a popup open;
    // normal double-click detection would fail due to coordinate space
    // change. (This actually keeps going afterwards, too).
    std::unique_ptr<ui::MouseEvent> adjusted_event;
    ui::MouseEvent* base_for_repeated = nullptr;
    if (monitor_ && event->target()) {
      auto location = event->target()->GetScreenLocation(*event);
      adjusted_event = CloneMouseEvent(event);
      adjusted_event->set_location(location);

      ui::MouseEvent* last_adjusted = OverallLastAdjusted(*event);
      if (last_adjusted &&
          last_adjusted->time_stamp() != adjusted_event->time_stamp() &&
          ui::MouseEvent::IsRepeatedClickEvent(*last_adjusted,
                                               *adjusted_event)) {
        base_for_repeated = last_adjusted;
      }
    }

    if (base_for_repeated) {
      adjusted_click_count =
          std::min(3, base_for_repeated->GetClickCount() + 1);
      // Make sure to store the new count with the saved adjusted event;
      // it matters if we're going to produce a triple-click.
      adjusted_event->SetClickCount(*adjusted_click_count);
      monitor_->ClearLastAdjusted();
    }
    last_adjusted_event_ = std::move(adjusted_event);

    // Make sure that focus gets grabbed; our strange arrangement can
    // prevent the normal ways of this happening.
    web_view_->RequestFocus();
  }

  // Dispatch to the NSView, so that the event takes the same path as events
  // AppKit delivers to it directly. That path includes AppKit-level state that
  // RenderWidgetHost::ForwardMouseEvent() would skip, e.g. keeping the first
  // responder in sync with the key tracking window in fullscreen
  // (crbug.com/563226016) and update cursor on mouse move
  // (crbug.com/430116472).
  if (content::RenderWidgetHostView* view =
          web_view_->GetWebContents()->GetRenderWidgetHostView()) {
    DispatchMouseEventToNativeView(*event, adjusted_click_count,
                                   view->GetNativeView().GetNativeNSView());
  }
}

void WebUIToolbarEventForwarder::AddedToWidget() {
  monitor_ = std::make_unique<PressMonitor>(control_delegate_->GetBrowser(),
                                            *web_view_);
}

void WebUIToolbarEventForwarder::RemovedFromWidget() {
  monitor_.reset();
}

bool WebUIToolbarEventForwarder::HaveOpenOmniboxPopup() {
  auto* bwi = control_delegate_->GetBrowser();
  if (!bwi) {
    return false;
  }
  // Note that this may be WebUILocationBar or LocationBarView, dependent
  // on flags.
  auto* location_bar = BrowserWindow::FromBrowser(bwi)->GetLocationBar();
  if (!location_bar) {
    return false;
  }
  return location_bar->GetOmniboxController()->IsPopupOpen();
}

ui::MouseEvent* WebUIToolbarEventForwarder::OverallLastAdjusted(
    const ui::MouseEvent& to_disregard) {
  auto* ours = last_adjusted_event_.get();
  auto* monitors = monitor_->LastAdjustedDisregarding(to_disregard);
  if (!ours) {
    return monitors;
  }
  if (!monitors) {
    return ours;
  }

  // In case of a tie, prefers our, since that's the copy that has the
  // count increased (for triple-click case).
  return ours->time_stamp() >= monitors->time_stamp() ? ours : monitors;
}
