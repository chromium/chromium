// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ai_overlay_dialog/page_context_monitor_android.h"

#include <memory>

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"

namespace ttc {

// static
std::unique_ptr<PageContextMonitor> PageContextMonitor::Create(
    BrowserWindowInterface& window,
    AiOverlayDialogPageHandler& page_handler) {
  return std::make_unique<PageContextMonitorAndroid>(window, page_handler);
}

PageContextMonitorAndroid::PageContextMonitorAndroid(
    BrowserWindowInterface& window,
    AiOverlayDialogPageHandler& page_handler)
    : PageContextMonitor(window, page_handler) {
  if (TabListInterface* tab_list = TabListInterface::From(&window)) {
    tab_list_observation_.Observe(tab_list);
    UpdateActiveTab();
  }
}

PageContextMonitorAndroid::~PageContextMonitorAndroid() = default;

std::optional<gfx::Point> PageContextMonitorAndroid::GetCursorPosition() const {
  if (!last_pointer_position_.has_value() || !web_contents()) {
    return std::nullopt;
  }
  gfx::Point pt = gfx::ToRoundedPoint(*last_pointer_position_);
  gfx::Rect tab_bounds(web_contents()->GetContainerBounds().size());
  if (!tab_bounds.Contains(pt)) {
    return std::nullopt;
  }
  return pt;
}

void PageContextMonitorAndroid::UpdateActiveTab() {
  TabListInterface* tab_list = tab_list_observation_.GetSource();
  tabs::TabInterface* tab = tab_list ? tab_list->GetActiveTab() : nullptr;
  SetActiveWebContents(tab ? tab->GetContents() : nullptr);
}

void PageContextMonitorAndroid::SetActiveWebContents(
    content::WebContents* contents) {
  last_pointer_position_.reset();
  OnActiveWebContentsChanged(contents);
  UpdateInputObservation();
}

void PageContextMonitorAndroid::UpdateInputObservation() {
  input_observation_.Reset();
  if (web_contents()) {
    input_observation_.Observe(
        web_contents()->GetRenderViewHost()->GetWidget());
  }
}

void PageContextMonitorAndroid::RenderViewHostChanged(
    content::RenderViewHost* old_host,
    content::RenderViewHost* new_host) {
  input_observation_.Reset();
  input_observation_.Observe(new_host->GetWidget());
}

void PageContextMonitorAndroid::OnInputEvent(
    const content::RenderWidgetHost& host,
    const blink::WebInputEvent& event,
    input::InputEventSource source) {
  if (blink::WebInputEvent::IsMouseEventType(event.GetType())) {
    const auto& mouse_event = static_cast<const blink::WebMouseEvent&>(event);
    last_pointer_position_ = mouse_event.PositionInWidget();
  } else if (blink::WebInputEvent::IsTouchEventType(event.GetType())) {
    const auto& touch = static_cast<const blink::WebTouchEvent&>(event);
    if (touch.touches_length > 0) {
      last_pointer_position_ = touch.touches[0].PositionInWidget();
    }
  }
}

void PageContextMonitorAndroid::OnActiveTabChanged(TabListInterface& tab_list,
                                                   tabs::TabInterface* tab) {
  SetActiveWebContents(tab ? tab->GetContents() : nullptr);
}

void PageContextMonitorAndroid::OnTabRemoved(TabListInterface& tab_list,
                                             tabs::TabInterface* tab,
                                             TabRemovedReason removed_reason) {
  UpdateActiveTab();
}

void PageContextMonitorAndroid::OnAllTabsAreClosing(
    TabListInterface& tab_list) {
  SetActiveWebContents(nullptr);
}

void PageContextMonitorAndroid::OnTabListDestroyed(TabListInterface& tab_list) {
  tab_list_observation_.Reset();
  SetActiveWebContents(nullptr);
}

}  // namespace ttc
