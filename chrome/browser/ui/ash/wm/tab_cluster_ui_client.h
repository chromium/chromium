// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_WM_TAB_CLUSTER_UI_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_WM_TAB_CLUSTER_UI_CLIENT_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

namespace ash {
class TabClusterUIItem;
class TabClusterUIController;
}  // namespace ash

namespace content {
class WebContents;
}  // namespace content

// TabClusterUIClient:
// Inherits from `TabStripModelObserver` to collect tab info from browser.
// `TabClusterUIClient` observes tab strip and generates
// `ash::TabClusterUIItem::Info` according to the tabs opened in the browser.
// The `ash::TabClusterUIItem` is created based on the info and sent to
// `ash::TabClusterUIController` for management.
class TabClusterUIClient : public TabStripModelObserver {
 public:
  explicit TabClusterUIClient(ash::TabClusterUIController* controller);
  TabClusterUIClient(TabClusterUIClient&) = delete;
  TabClusterUIClient& operator=(TabClusterUIClient&) = delete;

  ~TabClusterUIClient() override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;

 private:
  raw_ptr<ash::TabClusterUIController> controller_;
  BrowserTabStripTracker browser_tab_strip_tracker_;
  // A map from web contents to tab items.
  std::map<content::WebContents*,
           raw_ptr<ash::TabClusterUIItem, CtnExperimental>>
      contents_item_map_;
};

#endif  // CHROME_BROWSER_UI_ASH_WM_TAB_CLUSTER_UI_CLIENT_H_
