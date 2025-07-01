// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/window_management/multiple_web_contents_opened_notification_delegate.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"

namespace {

constexpr int kSettingsButtonIndex = 0;

void NavigateToAppSettings(Profile* profile, const webapps::AppId& app_id) {
  const web_app::WebApp* web_app =
      web_app::WebAppProvider::GetForWebApps(profile)
          ->registrar_unsafe()
          .GetAppById(app_id);
  if (!web_app) {
    return;
  }
  chrome::ShowSiteSettings(profile, web_app->scope());
}

}  // namespace

MultipleWebContentsOpenedNotificationDelegate::
    MultipleWebContentsOpenedNotificationDelegate(Profile* profile,
                                                  const webapps::AppId& app_id)
    : profile_(*profile), app_id_(app_id) {}

void MultipleWebContentsOpenedNotificationDelegate::Click(
    const std::optional<int>& button_index,
    const std::optional<std::u16string>& reply) {
  if (button_index == kSettingsButtonIndex) {
    NavigateToAppSettings(&profile_.get(), app_id_);
  }
}
