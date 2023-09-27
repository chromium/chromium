// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_SERVICE_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_SERVICE_H_

#include <unordered_map>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_observer.h"
#include "components/keyed_service/core/keyed_service.h"

class Browser;
class TabOrganizationSession;

// Provides an interface for getting Organizations for tabs.
class TabOrganizationService : public KeyedService {
 public:
  using BrowserSessionMap =
      std::unordered_map<Browser*, std::unique_ptr<TabOrganizationSession>>;
  TabOrganizationService();
  TabOrganizationService(const TabOrganizationService&) = delete;
  TabOrganizationService& operator=(const TabOrganizationService& other) =
      delete;
  ~TabOrganizationService() override;

  // Called when an organization triggering moment occurs. Creates a session for
  // the browser, if a session does not already exist.
  void OnTriggerOccured(Browser* browser);

  const BrowserSessionMap& browser_session_map() const {
    return browser_session_map_;
  }

  const TabOrganizationSession* GetSessionForBrowser(Browser* browser);

  void AddObserver(TabOrganizationObserver* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(TabOrganizationObserver* observer) {
    observers_.RemoveObserver(observer);
  }

 private:
  // mapping of browser to session.
  BrowserSessionMap browser_session_map_;

  // A list of the observers of a tab organization Service.
  base::ObserverList<TabOrganizationObserver>::Unchecked observers_;
};

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_SERVICE_H_
