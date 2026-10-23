// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_PAGE_CONTEXT_MONITOR_VIEWS_H_
#define CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_PAGE_CONTEXT_MONITOR_VIEWS_H_

#include "base/callback_list.h"
#include "chrome/browser/ui/webui/ai_overlay_dialog/page_context_monitor.h"

class BrowserWindowInterface;

namespace ttc {

class PageContextMonitorViews : public PageContextMonitor {
 public:
  PageContextMonitorViews(BrowserWindowInterface& window,
                          AiOverlayDialogPageHandler& page_handler);
  ~PageContextMonitorViews() override;

  std::optional<gfx::Point> GetCursorPosition() const override;

 private:
  void OnActiveTabChanged(BrowserWindowInterface* window);

  base::CallbackListSubscription active_tab_subscription_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_PAGE_CONTEXT_MONITOR_VIEWS_H_
