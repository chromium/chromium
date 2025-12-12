// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/window_management/isolated_web_apps_opened_tabs_counter_service_delegate.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_ui_manager_impl.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"

namespace web_app {

namespace {

constexpr int kSettingsButtonIndex = 0;
constexpr int kCloseWindowsButtonIndex = 1;

void NavigateToAppSettings(Profile* profile, const webapps::AppId& app_id) {
  web_app::WebAppProvider::GetForWebApps(profile)
      ->ui_manager()
      .ShowWebAppSettings(app_id);
}

}  // namespace

IsolatedWebAppsOpenedTabsCounterServiceDelegate::
    IsolatedWebAppsOpenedTabsCounterServiceDelegate(
        Profile* profile,
        const webapps::AppId& app_id,
        IsolatedWebAppsOpenedTabsCounterService::CloseWebContentsCallback
            close_web_contents_callback,
        IsolatedWebAppsOpenedTabsCounterService::
            NotificationAcknowledgedCallback notification_acknowledged_callback,
        IsolatedWebAppsOpenedTabsCounterService::CloseNotificationCallback
            close_notification_callback)
    : profile_(*profile),
      app_id_(app_id),
      close_web_contents_callback_(std::move(close_web_contents_callback)),
      notification_acknowledged_callback_(
          std::move(notification_acknowledged_callback)),
      close_notification_callback_(std::move(close_notification_callback)) {}

IsolatedWebAppsOpenedTabsCounterServiceDelegate::
    ~IsolatedWebAppsOpenedTabsCounterServiceDelegate() = default;

void IsolatedWebAppsOpenedTabsCounterServiceDelegate::Click(
    const std::optional<int>& button_index,
    const std::optional<std::u16string>& reply) {
  if (!button_index) {
    return;
  }

  switch (button_index.value()) {
    case kSettingsButtonIndex:
      NavigateToAppSettings(&profile_.get(), app_id_);
      break;
    case kCloseWindowsButtonIndex:
      close_web_contents_callback_.Run(app_id_);
      break;
  }
}

void IsolatedWebAppsOpenedTabsCounterServiceDelegate::Close(bool by_user) {
  if (by_user) {
    notification_acknowledged_callback_.Run(app_id_);
  }
}

}  // namespace web_app
