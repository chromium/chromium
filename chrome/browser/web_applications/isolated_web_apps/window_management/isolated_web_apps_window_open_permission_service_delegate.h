// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WINDOW_MANAGEMENT_ISOLATED_WEB_APPS_WINDOW_OPEN_PERMISSION_SERVICE_DELEGATE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WINDOW_MANAGEMENT_ISOLATED_WEB_APPS_WINDOW_OPEN_PERMISSION_SERVICE_DELEGATE_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/web_applications/isolated_web_apps/window_management/isolated_web_apps_window_open_permission_service.h"
#include "components/webapps/common/web_app_id.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

class Profile;

namespace web_app {

class IsolatedWebAppsWindowOpenPermissionServiceDelegate
    : public message_center::NotificationDelegate {
 public:
  IsolatedWebAppsWindowOpenPermissionServiceDelegate(
      Profile* profile,
      const webapps::AppId& app_id,
      IsolatedWebAppsWindowOpenPermissionService::
          NotificationAcknowledgedCallback notification_acknowledged_callback,
      IsolatedWebAppsWindowOpenPermissionService::CloseNotificationCallback
          close_notification_callback);

  IsolatedWebAppsWindowOpenPermissionServiceDelegate(
      const IsolatedWebAppsWindowOpenPermissionServiceDelegate&) = delete;
  IsolatedWebAppsWindowOpenPermissionServiceDelegate& operator=(
      const IsolatedWebAppsWindowOpenPermissionServiceDelegate&) = delete;

 protected:
  ~IsolatedWebAppsWindowOpenPermissionServiceDelegate() override;

  // message_center::NotificationDelegate:
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override;
  void Close(bool by_user) override;

 private:
  const raw_ref<Profile> profile_;
  const webapps::AppId app_id_;

  IsolatedWebAppsWindowOpenPermissionService::NotificationAcknowledgedCallback
      notification_acknowledged_callback_;
  IsolatedWebAppsWindowOpenPermissionService::CloseNotificationCallback
      close_notification_callback_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WINDOW_MANAGEMENT_ISOLATED_WEB_APPS_WINDOW_OPEN_PERMISSION_SERVICE_DELEGATE_H_
