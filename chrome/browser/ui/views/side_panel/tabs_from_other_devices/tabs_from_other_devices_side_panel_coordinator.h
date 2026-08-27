// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_TABS_FROM_OTHER_DEVICES_TABS_FROM_OTHER_DEVICES_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_TABS_FROM_OTHER_DEVICES_TABS_FROM_OTHER_DEVICES_SIDE_PANEL_COORDINATOR_H_

#include "base/memory/raw_ref.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;
class Profile;
class SidePanelRegistry;
class TabsFromOtherDevicesSidePanelMetrics;

// TabsFromOtherDevicesSidePanelCoordinator handles the creation and
// registration of the "Tabs from other devices" SidePanelEntry.
class TabsFromOtherDevicesSidePanelCoordinator {
 public:
  DECLARE_USER_DATA(TabsFromOtherDevicesSidePanelCoordinator);

  explicit TabsFromOtherDevicesSidePanelCoordinator(
      BrowserWindowInterface* browser,
      Profile* profile);

  // Returns the coordinator for `browser`, or null if it does not have one
  // (e.g. unsupported for the profile).
  static TabsFromOtherDevicesSidePanelCoordinator* From(
      BrowserWindowInterface* browser);
  TabsFromOtherDevicesSidePanelCoordinator(
      const TabsFromOtherDevicesSidePanelCoordinator&) = delete;
  TabsFromOtherDevicesSidePanelCoordinator& operator=(
      const TabsFromOtherDevicesSidePanelCoordinator&) = delete;
  ~TabsFromOtherDevicesSidePanelCoordinator();

  static bool IsSupported(Profile* profile);

  void CreateAndRegisterEntry(SidePanelRegistry* global_registry);

 private:
  ui::ScopedUnownedUserData<TabsFromOtherDevicesSidePanelCoordinator>
      scoped_unowned_user_data_;

  const raw_ref<BrowserWindowInterface> browser_;
  const raw_ref<Profile> profile_;
  std::unique_ptr<TabsFromOtherDevicesSidePanelMetrics> metrics_recorder_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_TABS_FROM_OTHER_DEVICES_TABS_FROM_OTHER_DEVICES_SIDE_PANEL_COORDINATOR_H_
