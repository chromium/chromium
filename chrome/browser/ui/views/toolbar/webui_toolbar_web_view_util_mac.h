// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TOOLBAR_WEB_VIEW_UTIL_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TOOLBAR_WEB_VIEW_UTIL_MAC_H_

#include <list>
#include <memory>

#include "ui/events/event.h"
#include "ui/views/event_monitor.h"

namespace views {
class WebView;
}  // namespace views

class BrowserWindowInterface;

// Records recent mouse presses in screen coordinates.
// Must be recreated when the browser window enters or
// leaves fullscreen.
class WebUIToolbarPressMonitor : public ui::EventObserver {
 public:
  explicit WebUIToolbarPressMonitor(BrowserWindowInterface* bwi,
                                    views::WebView& web_view);
  ~WebUIToolbarPressMonitor() override;

  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override;

  // Returns the most recent recorded mouse press that's not `to_disregard`.
  // May be nullptr if nothing relevant is recorded.
  ui::MouseEvent* LastAdjustedDisregarding(const ui::Event& to_disregard);

  void ClearLastAdjusted() { adjusted_events_.clear(); }

  static std::unique_ptr<ui::MouseEvent> CloneMouseEvent(
      const ui::MouseEvent* mouse_event);

 private:
  std::unique_ptr<views::EventMonitor> monitor_;

  // Recent mouse presses we got, with locations adjusted to screen coordinates.
  // This has more than one since if we're using an application monitor due to
  // fullscreen, we will have duplicates of things seen by the forwarder.
  std::list<std::unique_ptr<ui::MouseEvent>> adjusted_events_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_TOOLBAR_WEB_VIEW_UTIL_MAC_H_
