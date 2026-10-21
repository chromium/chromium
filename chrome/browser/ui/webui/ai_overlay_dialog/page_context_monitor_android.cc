// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ai_overlay_dialog/page_context_monitor_android.h"

#include <memory>

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/tabs/public/tab_interface.h"

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

void PageContextMonitorAndroid::UpdateActiveTab() {
  TabListInterface* tab_list = tab_list_observation_.GetSource();
  tabs::TabInterface* tab = tab_list ? tab_list->GetActiveTab() : nullptr;
  OnActiveWebContentsChanged(tab ? tab->GetContents() : nullptr);
}

void PageContextMonitorAndroid::OnActiveTabChanged(TabListInterface& tab_list,
                                                   tabs::TabInterface* tab) {
  OnActiveWebContentsChanged(tab ? tab->GetContents() : nullptr);
}

void PageContextMonitorAndroid::OnTabRemoved(TabListInterface& tab_list,
                                             tabs::TabInterface* tab,
                                             TabRemovedReason removed_reason) {
  UpdateActiveTab();
}

void PageContextMonitorAndroid::OnAllTabsAreClosing(
    TabListInterface& tab_list) {
  OnActiveWebContentsChanged(nullptr);
}

void PageContextMonitorAndroid::OnTabListDestroyed(TabListInterface& tab_list) {
  tab_list_observation_.Reset();
  OnActiveWebContentsChanged(nullptr);
}

}  // namespace ttc
