// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_SERVICE_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_SERVICE_H_

#include <unordered_map>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_observer.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_session.h"
#include "chrome/browser/ui/tabs/organization/trigger_observer.h"
#include "components/keyed_service/core/keyed_service.h"

class Browser;
class TabOrganizationSession;

namespace content {
class BrowserContext;
}

// Provides an interface for getting Organizations for tabs.
class TabOrganizationService : public KeyedService {
 public:
  using BrowserSessionMap =
      std::unordered_map<const Browser*,
                         std::unique_ptr<TabOrganizationSession>>;
  explicit TabOrganizationService(content::BrowserContext* browser_context);
  TabOrganizationService(const TabOrganizationService&) = delete;
  TabOrganizationService& operator=(const TabOrganizationService& other) =
      delete;
  ~TabOrganizationService() override;

  // Called when an organization triggering moment occurs. Creates a session for
  // the browser, if a session does not already exist.
  void OnTriggerOccured(const Browser* browser);

  // Notifies observers when a session from this service starts a request.
  void OnStartRequest(const TabOrganizationSession::ID session_id) const;

  const BrowserSessionMap& browser_session_map() const {
    return browser_session_map_;
  }

  const TabOrganizationSession* GetSessionForBrowser(
      const Browser* browser) const;
  TabOrganizationSession* GetSessionForBrowser(const Browser* browser);

  // Creates a new tab organization session, checking to ensure one does not
  // already exist for the browser. If callers are unsure whether there is an
  // existing session, they should first call GetSessionForBrowser to confirm.
  TabOrganizationSession* CreateSessionForBrowser(const Browser* browser);

  // Starts a request for the tab organization session that exists for the
  // browser, creating a new session if one does not already exists. Does not
  // start a request if one is already started.
  void StartRequest(const Browser* browser);

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

  TabOrganizationTriggerObserver trigger_observer_;
};

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_SERVICE_H_
