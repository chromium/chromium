// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_MULTI_PROFILE_BROWSER_STATUS_MONITOR_H_
#define CHROME_BROWSER_UI_ASH_SHELF_MULTI_PROFILE_BROWSER_STATUS_MONITOR_H_

#include "base/macros.h"
#include "chrome/browser/ui/ash/shelf/browser_status_monitor.h"

// MultiProfileBrowserStatusMonitor uses mainly the BrowserStatusMonitor
// with the addition that it creates and destroys launcher items for windowed
// V1 apps - upon creation as well as upon user switch.
class MultiProfileBrowserStatusMonitor : public BrowserStatusMonitor {
 public:
  explicit MultiProfileBrowserStatusMonitor(
      ChromeShelfController* shelf_controller);
  ~MultiProfileBrowserStatusMonitor() override;

  // BrowserStatusMonitor overrides.
  void ActiveUserChanged(const std::string& user_email) override;
  void AddV1AppToShelf(Browser* browser) override;
  void RemoveV1AppFromShelf(Browser* browser) override;

 private:
  typedef std::vector<Browser*> AppList;
  AppList app_list_;

  // Connect a V1 app to the launcher.
  void ConnectV1AppToLauncher(Browser* browser);

  // Disconnect a V1 app from the launcher.
  void DisconnectV1AppFromLauncher(Browser* browser);

  // The launcher controller which is associated with this object.
  ChromeShelfController* shelf_controller_;

  DISALLOW_COPY_AND_ASSIGN(MultiProfileBrowserStatusMonitor);
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_MULTI_PROFILE_BROWSER_STATUS_MONITOR_H_
