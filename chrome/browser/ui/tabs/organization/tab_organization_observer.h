// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_OBSERVER_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_OBSERVER_H_

class Browser;
class TabOrganizationSession;

class TabOrganizationObserver {
 public:
  // Called when all checks pass to be able to show the organization call to
  // action UI.
  virtual void OnToggleActionUIState(const Browser* browser, bool should_show) {
  }

  // Called when a session is created.
  virtual void OnSessionCreated(const Browser* browser,
                                TabOrganizationSession* session) {}

  // Called when the user accepts a suggested tab group.
  virtual void OnOrganizationAccepted(const Browser* browser) {}

  // Called when the user invokes the feature directly.
  virtual void OnUserInvokedFeature(const Browser* browser) {}
};

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_OBSERVER_H_
