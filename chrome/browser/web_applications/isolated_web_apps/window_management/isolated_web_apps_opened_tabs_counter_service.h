
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WINDOW_MANAGEMENT_ISOLATED_WEB_APPS_OPENED_TABS_COUNTER_SERVICE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WINDOW_MANAGEMENT_ISOLATED_WEB_APPS_OPENED_TABS_COUNTER_SERVICE_H_

#include <optional>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/webapps/common/web_app_id.h"

// Isolated Web Apps (IWAs) are granted the "Pop-ups and Redirects"
// content setting permission by default upon installation. As a result,
// these apps can open multiple new windows/tabs etc. programmatically
// (i.e., without a user gesture).
// To mitigate potential abuse of this permission and to not confuse the user,
// this service tracks the number of active `WebContents` (tabs or windows)
// opened by each IWA. When an IWA has opened more than one window, this service
// displays a notification. The notification informs the user that the app has
// opened multiple new windows/tabs and provides a button that directs them to
// the app's content settings page, giving them the option to revoke the pop-up
// permission.
//
// The service works by:
// 1. Observing all browsers associated with a specific profile.
// 2. Attaching a `TabStripModelObserver` to each browser's tab strip.
// 3. When a new tab is inserted, it checks if the tab's opener is a
//    non-policy-installed IWA.
// 4. If it is, the service increments a counter for that IWA and stores a
//    mapping from the new `WebContents` to the IWA's `AppId`.
// 5. When the count of opened windows for a specific IWA exceeds 1, a
//    notification is created and displayed.
// 6. As tabs are closed, the count is decremented. The notification is updated
//    if the count changes or removed if the count drops below 2.
class IsolatedWebAppsOpenedTabsCounterService : public KeyedService,
                                                public BrowserListObserver,
                                                public TabStripModelObserver {
 public:
  explicit IsolatedWebAppsOpenedTabsCounterService(Profile* profile);
  ~IsolatedWebAppsOpenedTabsCounterService() override;

  // KeyedService:
  void Shutdown() override;

 private:
  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  void HandleOpenerCountIfTracked(content::WebContents* contents);
  void HandleTabOrWindowClosure(content::WebContents* contents);
  std::optional<webapps::AppId> MaybeGetOpenerIsolatedWebAppId(
      content::WebContents* contents);

  void IncrementWindowCountForApp(const webapps::AppId& app_id);
  void DecrementWindowCountForApp(const webapps::AppId& app_id);

  void UpdateOrRemoveNotificationForOpener(const webapps::AppId& app_id);
  void CreateAndDisplayNotification(const webapps::AppId& app_id,
                                    int current_window_count);

  Profile* profile() { return &profile_.get(); }

  const raw_ref<Profile> profile_;
  base::flat_map<webapps::AppId, int> app_window_counts_;
  base::flat_map<content::WebContents*, webapps::AppId> opened_by_app_map_;
  base::ScopedObservation<BrowserList, BrowserListObserver>
      browser_list_observation_{this};
};

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WINDOW_MANAGEMENT_ISOLATED_WEB_APPS_OPENED_TABS_COUNTER_SERVICE_H_
