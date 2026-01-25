// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WINDOW_MANAGEMENT_ISOLATED_WEB_APPS_WINDOW_OPEN_PERMISSION_SERVICE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WINDOW_MANAGEMENT_ISOLATED_WEB_APPS_WINDOW_OPEN_PERMISSION_SERVICE_H_

#include <map>
#include <memory>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolation_data.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/web_app_provider.h"
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
class IsolatedWebAppsWindowOpenPermissionService : public KeyedService {
 public:
  using NotificationAcknowledgedCallback =
      base::RepeatingCallback<void(const webapps::AppId&)>;
  using CloseNotificationCallback =
      base::RepeatingCallback<void(const webapps::AppId&)>;

  explicit IsolatedWebAppsWindowOpenPermissionService(Profile* profile);
  ~IsolatedWebAppsWindowOpenPermissionService() override;

  // KeyedService:
  void Shutdown() override;

  // Called by the `web_app::NavigationCapturingProcess` when a new WebContents
  // is created by an IWA.
  void OnWebContentsCreated(const webapps::AppId& opener_app_id);

  void OnNotificationAcknowledged(const webapps::AppId& app_id);
  void CloseNotification(const webapps::AppId& app_id);

 private:
  void RetrieveNotificationStates();
  void OnAllAppsLockAcquiredForStateRetrieval(web_app::AllAppsLock& lock,
                                              base::DictValue& debug_value);

  void RegisterFirstTimeActiveAppNotification(const webapps::AppId& app_id);
  void CreateAndDisplayNotification(const webapps::AppId& app_id);

  void PersistNotificationState(
      const webapps::AppId& app_id,
      const web_app::IsolationData::OpenedTabsCounterNotificationState&
          current_notification_state);

  Profile* profile() { return &profile_.get(); }
  WebAppProvider* provider() const { return provider_; }

  const raw_ref<Profile> profile_;
  const raw_ptr<WebAppProvider> provider_;

  // In-memory cache of notification states, loaded on startup.
  std::map<webapps::AppId,
           web_app::IsolationData::OpenedTabsCounterNotificationState>
      notification_states_cache_;

  // Set of AppIds for which a notification is currently active.
  base::flat_set<webapps::AppId> apps_with_active_notifications_;

  std::map<webapps::AppId, base::OneShotTimer> dismissal_timers_;

  base::WeakPtrFactory<IsolatedWebAppsWindowOpenPermissionService>
      weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WINDOW_MANAGEMENT_ISOLATED_WEB_APPS_WINDOW_OPEN_PERMISSION_SERVICE_H_
