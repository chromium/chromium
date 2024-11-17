// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/apps/android_apps_handler.h"

#include "ash/components/arc/app/arc_app_constants.h"
#include "base/functional/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/browser/web_contents.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"

namespace ash::settings {

AndroidAppsHandler::AndroidAppsHandler(Profile* profile,
                                       apps::AppServiceProxy* app_service_proxy)
    : profile_(profile), app_service_proxy_(app_service_proxy) {}

AndroidAppsHandler::~AndroidAppsHandler() {}

void AndroidAppsHandler::RegisterMessages() {
  // Note: requestAndroidAppsInfo must be called before observers will be added.
  web_ui()->RegisterMessageCallback(
      "requestAndroidAppsInfo",
      base::BindRepeating(&AndroidAppsHandler::HandleRequestAndroidAppsInfo,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "showAndroidAppsSettings",
      base::BindRepeating(&AndroidAppsHandler::ShowAndroidAppsSettings,
                          weak_ptr_factory_.GetWeakPtr()));
}

void AndroidAppsHandler::OnJavascriptAllowed() {
  ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile_);
  if (arc_prefs) {
    arc_prefs_observation_.Observe(arc_prefs);
    // arc::ArcSessionManager is associated with primary profile.
    arc_session_manager_observation_.Observe(arc::ArcSessionManager::Get());
  }
}

void AndroidAppsHandler::OnJavascriptDisallowed() {
  arc_prefs_observation_.Reset();
  arc_session_manager_observation_.Reset();
}

void AndroidAppsHandler::OnAppRegistered(
    const std::string& app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  HandleAppChanged(app_id);
}

void AndroidAppsHandler::OnAppStatesChanged(
    const std::string& app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  HandleAppChanged(app_id);
}

void AndroidAppsHandler::OnAppRemoved(const std::string& app_id) {
  HandleAppChanged(app_id);
}

void AndroidAppsHandler::HandleAppChanged(const std::string& app_id) {
  if (app_id != arc::kSettingsAppId) {
    return;
  }
  SendAndroidAppsInfo();
}

void AndroidAppsHandler::OnArcPlayStoreEnabledChanged(bool enabled) {
  SendAndroidAppsInfo();
}

base::Value::Dict AndroidAppsHandler::BuildAndroidAppsInfo() {
  base::Value::Dict info;
  info.Set("playStoreEnabled", arc::IsArcPlayStoreEnabledForProfile(profile_));
  const ArcAppListPrefs* arc_apps_pref = ArcAppListPrefs::Get(profile_);
  // TODO(khmel): Inverstigate why in some browser tests
  // playStoreEnabled is true but arc_apps_pref is not set.
  info.Set("settingsAppAvailable",
           arc_apps_pref && arc_apps_pref->IsRegistered(arc::kSettingsAppId));
  return info;
}

void AndroidAppsHandler::HandleRequestAndroidAppsInfo(
    const base::Value::List& args) {
  AllowJavascript();
  SendAndroidAppsInfo();
}

void AndroidAppsHandler::SendAndroidAppsInfo() {
  FireWebUIListener("android-apps-info-update", BuildAndroidAppsInfo());
}

void AndroidAppsHandler::ShowAndroidAppsSettings(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  bool activated_from_keyboard = false;
  if (args[0].is_bool()) {
    activated_from_keyboard = args[0].GetBool();
  }
  int flags = activated_from_keyboard ? ui::EF_NONE : ui::EF_LEFT_MOUSE_BUTTON;

  app_service_proxy_->Launch(
      arc::kSettingsAppId, flags, apps::LaunchSource::kFromParentalControls,
      std::make_unique<apps::WindowInfo>(GetDisplayIdForCurrentProfile()));
}

int64_t AndroidAppsHandler::GetDisplayIdForCurrentProfile() {
  // Settings in secondary profile cannot access ARC.
  DCHECK(arc::IsArcAllowedForProfile(profile_));
  return display::Screen::GetScreen()
      ->GetDisplayNearestView(web_ui()->GetWebContents()->GetNativeView())
      .id();
}

}  // namespace ash::settings
