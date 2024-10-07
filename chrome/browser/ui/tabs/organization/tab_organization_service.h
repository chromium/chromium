// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_SERVICE_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_SERVICE_H_

#include <unordered_map>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/tabs/organization/metrics.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_observer.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_session.h"
#include "chrome/browser/ui/tabs/organization/trigger_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/keyed_service/core/keyed_service.h"

class Browser;
class TabOrganizationSession;
class TabSensitivityCache;

namespace content {
class BrowserContext;
}

namespace tabs {
class TabModel;
}

// Provides an interface for getting Organizations for tabs.
class TabOrganizationService
    : public KeyedService,
      public TabStripModelObserver {
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

  const BrowserSessionMap& browser_session_map() const {
    return browser_session_map_;
  }

  const TabSensitivityCache* tab_sensitivity_cache() const {
    return tab_sensitivity_cache_.get();
  }

  const TabOrganizationSession* GetSessionForBrowser(
      const Browser* browser) const;
  TabOrganizationSession* GetSessionForBrowser(const Browser* browser);

  // Creates a new tab organization session, checking to ensure one does not
  // already exist for the browser. If callers are unsure whether there is an
  // existing session, they should first call GetSessionForBrowser to confirm.
  TabOrganizationSession* CreateSessionForBrowser(
      const Browser* browser,
      const TabOrganizationEntryPoint entrypoint,
      const tabs::TabModel* base_session_tab = nullptr);

  // If the session exists, destroys the session, calls CreateSessionForBrowser.
  TabOrganizationSession* ResetSessionForBrowser(
      const Browser* browser,
      const TabOrganizationEntryPoint entrypoint,
      const tabs::TabModel* base_session_tab = nullptr);

  // Convenience method that resets the session, starts a request if not in the
  // first run experience, and opens the Organization UI.
  void RestartSessionAndShowUI(
      const Browser* browser,
      const TabOrganizationEntryPoint entrypoint,
      const tabs::TabModel* base_session_tab = nullptr);

  // Allows for other User actions to open up the Organization UI.
  void OnUserInvokedFeature(const Browser* browser);

  void AcceptTabOrganization(Browser* browser,
                             TabOrganization::ID session_id,
                             TabOrganization::ID organization_id);

  // Called when the proactive nudge button is clicked.
  void OnActionUIAccepted(const Browser* browser);

  // Called when the close button on the proactive nudge UI is clicked.
  void OnActionUIDismissed(const Browser* browser);

  // Checks if the user is in the first run experience, and starts a request if
  // not.
  void StartRequestIfNotFRE(const Browser* browser,
                            const TabOrganizationEntryPoint entrypoint);

  // Starts a request for the tab organization session that exists for the
  // browser, creating a new session if one does not already exists. Does not
  // start a request if one is already started.
  void StartRequest(const Browser* browser,
                    const TabOrganizationEntryPoint entrypoint);

  void AddObserver(TabOrganizationObserver* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(TabOrganizationObserver* observer) {
    observers_.RemoveObserver(observer);
  }

  bool HasObserver(TabOrganizationObserver* observer) {
    return observers_.HasObserver(observer);
  }

  // TabStripModelObserver.
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnTabGroupChanged(const TabGroupChange& change) override;

  // Returns true if the profile that owns this service should be able to create
  // requests to the TabOrganizationRequest object, otherwise returns false.
  bool CanStartRequest() const;

 private:
  void RemoveBrowserFromSessionMap(const Browser* browser);
  const Browser* GetBrowserForTabStripModel(
      const TabStripModel* tab_strip_model);

  // mapping of browser to session.
  BrowserSessionMap browser_session_map_;

  // A list of the observers of a tab organization Service.
  base::ObserverList<TabOrganizationObserver>::Unchecked observers_;

  std::unique_ptr<TabSensitivityCache> tab_sensitivity_cache_;
  std::unique_ptr<BackoffLevelProvider> trigger_backoff_;
  std::unique_ptr<TabOrganizationTriggerObserver> trigger_observer_;

  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_SERVICE_H_
