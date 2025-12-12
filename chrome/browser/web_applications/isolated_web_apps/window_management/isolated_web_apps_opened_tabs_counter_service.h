// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WINDOW_MANAGEMENT_ISOLATED_WEB_APPS_OPENED_TABS_COUNTER_SERVICE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WINDOW_MANAGEMENT_ISOLATED_WEB_APPS_OPENED_TABS_COUNTER_SERVICE_H_

#include <map>
#include <memory>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolation_data.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}  // namespace content

namespace web_app {

// Isolated Web Apps (IWAs) are granted the "Pop-ups and Redirects"
// permission by default upon installation. As a result,
// these apps can open multiple new windows/tabs programmatically.
// To mitigate potential abuse of this permission and to not confuse the user,
// this service tracks `WebContents` (tabs or windows) opened by each
// non-policy-installed IWA to detect a burst of activity.
//
// The service works by:
// 1. Being notified via the `OnWebContentsCreated` method when an IWA creates a
//    new `WebContents`. It records the creation timestamp for that tab.
// 2. Attaching an observer to the new `WebContents` to monitor its lifecycle.
// 3. Each time a new tab is opened, it checks how many tabs were opened by the
//    same app within a short time window (5 seconds). If the count
//    reaches a certain threshold (currently equal to 3), a notification is
//    displayed.
// 4. The notification informs the user and provides actions to either close all
//    tabs opened by the app or manage its "Pop-ups and Redirects" permission.
// 5. When a tracked `WebContents` is destroyed, its timestamp is removed. The
//    notification remains visible
class IsolatedWebAppsOpenedTabsCounterService : public KeyedService {
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

  // Called by the `web_app::NavigationCapturingProcess` when a new WebContents
  // is created by an IWA.
  void OnWebContentsCreated(const webapps::AppId& opener_app_id,
                            content::WebContents* new_contents,
                            base::Time navigation_start_time);

  void OnNotificationAcknowledged(const webapps::AppId& app_id);
  void CloseNotification(const webapps::AppId& app_id);

 private:
  FRIEND_TEST_ALL_PREFIXES(
      IsolatedWebAppsOpenedTabsCounterServiceBrowserTest,
      ClickCloseWindowsButtonClosesChildWindowsAndNotification);
  FRIEND_TEST_ALL_PREFIXES(IsolatedWebAppsOpenedTabsCounterServiceBrowserTest,
                           ForceInstalledIwaNeverShowsNotification);
  class TabObserver : public content::WebContentsObserver {
   public:
    TabObserver(content::WebContents* web_contents,
                IsolatedWebAppsOpenedTabsCounterService* service)
        : content::WebContentsObserver(web_contents), service_(service) {}
    ~TabObserver() override = default;

    // content::WebContentsObserver:
    void WebContentsDestroyed() override;

   private:
    raw_ptr<IsolatedWebAppsOpenedTabsCounterService> service_;
  };

  // Holds data about a WebContents opened by an IWA.
  struct TrackedTabData;

  void RetrieveNotificationStates();
  void OnAllAppsLockAcquiredForStateRetrieval(web_app::AllAppsLock& lock,
                                              base::Value::Dict& debug_value);

  // Called by `TabObserver` when a tracked WebContents is destroyed.
  void HandleTabClosure(content::WebContents* contents);

  void AddTabTimestampForApp(const webapps::AppId& app_id,
                             base::Time timestamp);
  void RemoveTabTimestampForApp(const webapps::AppId& app_id,
                                base::Time timestamp);

  void UpdateOrRemoveNotificationForOpener(const webapps::AppId& app_id);
  void RegisterFirstTimeActiveAppNotification(const webapps::AppId& app_id);
  void CreateAndDisplayNotification(const webapps::AppId& app_id,
                                    int current_tab_count);
  void CloseAllWebContentsOpenedByApp(const webapps::AppId& app_id);

  void PersistNotificationState(const webapps::AppId& app_id);

  Profile* profile() { return &profile_.get(); }

  const raw_ref<Profile> profile_;

  base::flat_map<webapps::AppId, std::vector<base::Time>> app_tab_timestamps_;

  // Tracks WebContents opened by IWAs, mapping each to its opener's AppId
  // and an observer that handles its destruction.
  base::flat_map<content::WebContents*, TrackedTabData> tracked_tabs_;

  // In-memory cache of notification states, loaded on startup.
  std::map<webapps::AppId,
           web_app::IsolationData::OpenedTabsCounterNotificationState>
      notification_states_cache_;

  // Set of AppIds for which a notification is currently active.
  base::flat_set<webapps::AppId> apps_with_active_notifications_;

  base::WeakPtrFactory<IsolatedWebAppsOpenedTabsCounterService>
      weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WINDOW_MANAGEMENT_ISOLATED_WEB_APPS_OPENED_TABS_COUNTER_SERVICE_H_
