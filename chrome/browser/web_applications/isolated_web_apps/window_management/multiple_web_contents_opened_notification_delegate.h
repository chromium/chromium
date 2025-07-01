// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WINDOW_MANAGEMENT_MULTIPLE_WEB_CONTENTS_OPENED_NOTIFICATION_DELEGATE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WINDOW_MANAGEMENT_MULTIPLE_WEB_CONTENTS_OPENED_NOTIFICATION_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/web_applications/isolated_web_apps/window_management/multiple_web_contents_opened_service.h"
#include "components/webapps/common/web_app_id.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

class Profile;

class MultipleWebContentsOpenedNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  MultipleWebContentsOpenedNotificationDelegate(
      Profile* profile,
      const webapps::AppId& app_id,
      MultipleWebContentsOpenedService::CloseWebContentsCallback
          close_web_contents_callback,
      MultipleWebContentsOpenedService::NotificationAcknowledgedCallback
          notification_acknowledged_callback,
      MultipleWebContentsOpenedService::CloseNotificationCallback
          close_notification_callback);

  MultipleWebContentsOpenedNotificationDelegate(
      const MultipleWebContentsOpenedNotificationDelegate&) = delete;
  MultipleWebContentsOpenedNotificationDelegate& operator=(
      const MultipleWebContentsOpenedNotificationDelegate&) = delete;

 protected:
  ~MultipleWebContentsOpenedNotificationDelegate() override;

  // message_center::NotificationDelegate:
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override;
  void Close(bool by_user) override;

 private:
  const raw_ref<Profile> profile_;
  const webapps::AppId app_id_;
  MultipleWebContentsOpenedService::CloseWebContentsCallback
      close_web_contents_callback_;
  MultipleWebContentsOpenedService::NotificationAcknowledgedCallback
      notification_acknowledged_callback_;
  MultipleWebContentsOpenedService::CloseNotificationCallback
      close_notification_callback_;
};

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WINDOW_MANAGEMENT_MULTIPLE_WEB_CONTENTS_OPENED_NOTIFICATION_DELEGATE_H_
