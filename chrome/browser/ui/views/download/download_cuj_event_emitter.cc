// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/download_cuj_event_emitter.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/views/interaction/element_tracker_views.h"

namespace download {

bool EmitElementTrackerEvent(BrowserWindowInterface* browser,
                             ui::CustomElementEventType event_type) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view) {
    return false;
  }
  return views::ElementTrackerViews::GetInstance()->NotifyCustomEvent(
      event_type, browser_view);
}

}  // namespace download
