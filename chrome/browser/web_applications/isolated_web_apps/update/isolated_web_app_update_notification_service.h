// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_UPDATE_ISOLATED_WEB_APP_UPDATE_NOTIFICATION_SERVICE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_UPDATE_ISOLATED_WEB_APP_UPDATE_NOTIFICATION_SERVICE_H_

#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/isolated_web_app_apply_update_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/update/isolated_web_app_update_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "components/webapps/common/web_app_id.h"

class Profile;

namespace message_center {
class Notification;
}  // namespace message_center

namespace web_app {

class WebAppProvider;

class IsolatedWebAppUpdateNotificationService
    : public WebAppInstallManagerObserver,
      public IsolatedWebAppUpdateManager::Observer {
 public:
  IsolatedWebAppUpdateNotificationService(Profile& profile,
                                          WebAppProvider& provider);
  ~IsolatedWebAppUpdateNotificationService() override;

  // Displays a system notification informing the user that an update for
  // `app_id` has been downloaded and is waiting for the app to close.
  // Clicking "Restart app" on the notification closes open windows and
  // relaunches the updated app.
  void ShowUpdatePendingNotification(const webapps::AppId& app_id);

  // Closes any displayed update notification for `app_id`.
  void CloseNotification(const webapps::AppId& app_id);

  IsolatedWebAppUpdateNotificationService(
      const IsolatedWebAppUpdateNotificationService&) = delete;
  IsolatedWebAppUpdateNotificationService& operator=(
      const IsolatedWebAppUpdateNotificationService&) = delete;

 private:
  // WebAppInstallManagerObserver:
  void OnWebAppUninstalled(
      const webapps::AppId& app_id,
      webapps::WebappUninstallSource uninstall_source) override;

  // IsolatedWebAppUpdateManager::Observer:
  void OnUpdateApplyTaskCompleted(
      const webapps::AppId& app_id,
      IsolatedWebAppApplyUpdateCommandResult status) override;

  std::unique_ptr<message_center::Notification> CreateUpdatePendingNotification(
      const webapps::AppId& app_id);

  void OnNotificationClick(const webapps::AppId& app_id,
                           std::optional<int> button_index);

  const raw_ref<Profile> profile_;
  const raw_ref<WebAppProvider> provider_;

  base::flat_set<webapps::AppId> restarted_apps_;

  base::ScopedObservation<WebAppInstallManager, WebAppInstallManagerObserver>
      install_manager_observation_{this};
  base::ScopedObservation<IsolatedWebAppUpdateManager,
                          IsolatedWebAppUpdateManager::Observer>
      update_manager_observation_{this};

  base::WeakPtrFactory<IsolatedWebAppUpdateNotificationService> weak_factory_{
      this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_UPDATE_ISOLATED_WEB_APP_UPDATE_NOTIFICATION_SERVICE_H_
