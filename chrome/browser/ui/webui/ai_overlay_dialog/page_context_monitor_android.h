// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_PAGE_CONTEXT_MONITOR_ANDROID_H_
#define CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_PAGE_CONTEXT_MONITOR_ANDROID_H_

#include "base/scoped_observation.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/tab_list/tab_list_interface_observer.h"
#include "chrome/browser/ui/webui/ai_overlay_dialog/page_context_monitor.h"

class BrowserWindowInterface;

namespace ttc {

class PageContextMonitorAndroid : public PageContextMonitor,
                                  public TabListInterfaceObserver {
 public:
  PageContextMonitorAndroid(BrowserWindowInterface& window,
                            AiOverlayDialogPageHandler& page_handler);
  ~PageContextMonitorAndroid() override;

 private:
  // Re-reads the active tab from the observed tab list.
  void UpdateActiveTab();

  // TabListInterfaceObserver:
  void OnActiveTabChanged(TabListInterface& tab_list,
                          tabs::TabInterface* tab) override;
  void OnTabRemoved(TabListInterface& tab_list,
                    tabs::TabInterface* tab,
                    TabRemovedReason removed_reason) override;
  void OnAllTabsAreClosing(TabListInterface& tab_list) override;
  void OnTabListDestroyed(TabListInterface& tab_list) override;

  base::ScopedObservation<TabListInterface, TabListInterfaceObserver>
      tab_list_observation_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_PAGE_CONTEXT_MONITOR_ANDROID_H_
