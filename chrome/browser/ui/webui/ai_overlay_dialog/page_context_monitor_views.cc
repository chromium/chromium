// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ai_overlay_dialog/page_context_monitor_views.h"

#include <memory>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/display/screen.h"

namespace ttc {

// static
std::unique_ptr<PageContextMonitor> PageContextMonitor::Create(
    BrowserWindowInterface& window,
    AiOverlayDialogPageHandler& page_handler) {
  return std::make_unique<PageContextMonitorViews>(window, page_handler);
}

PageContextMonitorViews::PageContextMonitorViews(
    BrowserWindowInterface& window,
    AiOverlayDialogPageHandler& page_handler)
    : PageContextMonitor(window, page_handler) {
  active_tab_subscription_ = window.RegisterActiveTabDidChange(
      base::BindRepeating(&PageContextMonitorViews::OnActiveTabChanged,
                          base::Unretained(this)));
  OnActiveTabChanged(&window);
}

PageContextMonitorViews::~PageContextMonitorViews() = default;

void PageContextMonitorViews::OnActiveTabChanged(
    BrowserWindowInterface* window) {
  CHECK_EQ(window, &this->window());

  tabs::TabInterface* active_tab = this->window().GetActiveTabInterface();
  OnActiveWebContentsChanged(active_tab ? active_tab->GetContents() : nullptr);
}

std::optional<gfx::Point> PageContextMonitorViews::GetCursorPosition() const {
  display::Screen* screen = display::Screen::Get();
  if (!screen || !web_contents()) {
    return std::nullopt;
  }
  gfx::Point cursor_screen = screen->GetCursorScreenPoint();
  gfx::Rect tab_bounds = web_contents()->GetContainerBounds();
  if (!tab_bounds.Contains(cursor_screen)) {
    return std::nullopt;
  }
  return cursor_screen - tab_bounds.OffsetFromOrigin();
}

}  // namespace ttc
