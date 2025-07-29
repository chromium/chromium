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
#include "chrome/browser/web_applications/isolated_web_apps/isolation_data.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

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
  using CloseWebContentsCallback =
      base::RepeatingCallback<void(const webapps::AppId&)>;
  using NotificationAcknowledgedCallback =
      base::RepeatingCallback<void(const webapps::AppId&)>;
  using CloseNotificationCallback = base::RepeatingClosure;

  explicit IsolatedWebAppsOpenedTabsCounterService(Profile* profile);
  ~IsolatedWebAppsOpenedTabsCounterService() override;

  // KeyedService:
  void Shutdown() override;

  void OnNotificationAcknowledged(const webapps::AppId& app_id);
  void CloseNotification(const webapps::AppId& app_id);

 private:
  FRIEND_TEST_ALL_PREFIXES(
      IsolatedWebAppsOpenedTabsCounterServiceBrowserTest,
      ClickCloseWindowsButtonClosesChildWindowsAndNotification);

  void RetrieveNotificationStates();
  void OnAllAppsLockAcquiredForStateRetrieval(web_app::AllAppsLock& lock,
                                              base::Value::Dict& debug_value);

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  void HandleOpenerCountIfTracked(content::WebContents* contents);
  void HandleTabClosure(content::WebContents* contents);
  std::optional<webapps::AppId> MaybeGetOpenerIsolatedWebAppId(
      content::WebContents* contents);

  void IncrementTabCountForApp(const webapps::AppId& app_id);
  void DecrementTabCountForApp(const webapps::AppId& app_id);

  void UpdateOrRemoveNotificationForOpener(const webapps::AppId& app_id);
  void CreateAndDisplayNotification(const webapps::AppId& app_id,
                                    int current_window_count);
  void CloseAllWebContentsOpenedByApp(const webapps::AppId& app_id);

  void PersistNotificationState(const webapps::AppId& app_id);

  Profile* profile() { return &profile_.get(); }

  const raw_ref<Profile> profile_;
  base::flat_map<webapps::AppId, int> app_tab_counts_;

  base::flat_map<content::WebContents*, webapps::AppId> opened_by_app_map_;
  // These are loaded in `RetrieveNotificationStates` method, and saved as an
  // in-memory cache. After modifying, `PersistNotificationState` should always
  // be called.
  std::map<webapps::AppId,
           web_app::IsolationData::OpenedTabsCounterNotificationState>
      notification_states_cache_;

  base::flat_set<webapps::AppId> apps_with_active_notifications_;

  base::ScopedObservation<BrowserList, BrowserListObserver>
      browser_list_observation_{this};
  base::WeakPtrFactory<IsolatedWebAppsOpenedTabsCounterService>
      weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WINDOW_MANAGEMENT_ISOLATED_WEB_APPS_OPENED_TABS_COUNTER_SERVICE_H_
