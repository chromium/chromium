// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_OBSERVER_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_OBSERVER_H_

class Browser;

class TabOrganizationObserver {
 public:
  // Called when all checks pass to be able to show the organization call to
  // action UI.
  virtual void OnToggleActionUIState(Browser* browser, bool should_show) {}
};

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_OBSERVER_H_
