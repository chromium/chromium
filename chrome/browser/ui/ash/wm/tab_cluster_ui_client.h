// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_WM_TAB_CLUSTER_UI_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_WM_TAB_CLUSTER_UI_CLIENT_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/browser_delegate/browser_controller.h"

namespace ash {
class BrowserDelegate;
class TabClusterUIItem;
class TabClusterUIController;
}  // namespace ash

namespace content {
class WebContents;
}  // namespace content

// TabClusterUIClient:
// Collects tab info from browser.
// `TabClusterUIClient` observes browser tabs and generates
// `ash::TabClusterUIItem::Info` according to the tabs opened in the browser.
// The `ash::TabClusterUIItem` is created based on the info and sent to
// `ash::TabClusterUIController` for management.
class TabClusterUIClient : public ash::BrowserController::TabObserver {
 public:
  explicit TabClusterUIClient(ash::TabClusterUIController* controller);
  TabClusterUIClient(TabClusterUIClient&) = delete;
  TabClusterUIClient& operator=(TabClusterUIClient&) = delete;

  ~TabClusterUIClient() override;

  // ash::BrowserController::TabObserver:
  void OnTabInserted(ash::BrowserDelegate* browser,
                     content::WebContents* contents) override;
  void OnTabRemoved(ash::BrowserDelegate* browser,
                    content::WebContents* contents,
                    bool will_delete) override;
  void OnTabReplaced(ash::BrowserDelegate* browser,
                     content::WebContents* old_contents,
                     content::WebContents* new_contents) override;
  void OnTabLoadingStateChanged(ash::BrowserDelegate* browser,
                                content::WebContents* contents) override;

 private:
  class TrackedTab;

  raw_ptr<ash::TabClusterUIController> controller_;
  base::ScopedObservation<ash::BrowserController,
                          ash::BrowserController::TabObserver>
      tab_observation_{this};
  // A map from web contents to tracked tab observers.
  std::map<content::WebContents*, std::unique_ptr<TrackedTab>> tabs_;
};

#endif  // CHROME_BROWSER_UI_ASH_WM_TAB_CLUSTER_UI_CLIENT_H_
