// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WINDOW_MANAGEMENT_ISOLATED_WEB_APPS_OPENED_TABS_COUNTER_SERVICE_DELEGATE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WINDOW_MANAGEMENT_ISOLATED_WEB_APPS_OPENED_TABS_COUNTER_SERVICE_DELEGATE_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/web_applications/isolated_web_apps/window_management/isolated_web_apps_opened_tabs_counter_service.h"
#include "components/webapps/common/web_app_id.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

class Profile;

namespace web_app {

class IsolatedWebAppsOpenedTabsCounterServiceDelegate
    : public message_center::NotificationDelegate {
 public:
  IsolatedWebAppsOpenedTabsCounterServiceDelegate(
      Profile* profile,
      const webapps::AppId& app_id,
      IsolatedWebAppsOpenedTabsCounterService::CloseWebContentsCallback
          close_web_contents_callback,
      IsolatedWebAppsOpenedTabsCounterService::NotificationAcknowledgedCallback
          notification_acknowledged_callback,
      IsolatedWebAppsOpenedTabsCounterService::CloseNotificationCallback
          close_notification_callback);

  IsolatedWebAppsOpenedTabsCounterServiceDelegate(
      const IsolatedWebAppsOpenedTabsCounterServiceDelegate&) = delete;
  IsolatedWebAppsOpenedTabsCounterServiceDelegate& operator=(
      const IsolatedWebAppsOpenedTabsCounterServiceDelegate&) = delete;

 protected:
  ~IsolatedWebAppsOpenedTabsCounterServiceDelegate() override;

  // message_center::NotificationDelegate:
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override;
  void Close(bool by_user) override;

 private:
  const raw_ref<Profile> profile_;
  const webapps::AppId app_id_;

  IsolatedWebAppsOpenedTabsCounterService::CloseWebContentsCallback
      close_web_contents_callback_;
  IsolatedWebAppsOpenedTabsCounterService::NotificationAcknowledgedCallback
      notification_acknowledged_callback_;
  IsolatedWebAppsOpenedTabsCounterService::CloseNotificationCallback
      close_notification_callback_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WINDOW_MANAGEMENT_ISOLATED_WEB_APPS_OPENED_TABS_COUNTER_SERVICE_DELEGATE_H_
