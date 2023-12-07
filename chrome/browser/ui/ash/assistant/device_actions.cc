// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/device_actions.h"

#include <string>
#include <utility>
#include <vector>

#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/bluetooth_config_service.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/app_list/arc/intent.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/technology_state_controller.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "ui/display/types/display_constants.h"

using ::ash::NetworkHandler;
using ::ash::NetworkTypePattern;
using ::ash::TechnologyStateController;
using ::ash::assistant::AndroidAppInfo;
using ::ash::assistant::AppStatus;

namespace {

constexpr char kIntentPrefix[] = "#Intent";
constexpr char kAction[] = "action";
constexpr char kPackage[] = "package";
constexpr char kLaunchFlags[] = "launchFlags";
constexpr char kEndSuffix[] = "end";

std::optional<std::string> GetActivity(const std::string& package_name) {
  auto* prefs = ArcAppListPrefs::Get(ProfileManager::GetActiveUserProfile());
  if (!prefs) {
    LOG(ERROR) << "ArcAppListPrefs is not available.";
    return std::nullopt;
  }
  std::string app_id = prefs->GetAppIdByPackageName(package_name);

  if (!app_id.empty()) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
    return std::optional<std::string>(app_info->activity);
  }

  return std::nullopt;
}

std::string GetLaunchIntent(const AndroidAppInfo& app_info) {
  auto& package_name = app_info.package_name;
  if (app_info.intent.empty() || app_info.action.empty()) {
    // No action or data specified. Using launch intent from ARC.
    return arc::GetLaunchIntent(package_name,
                                GetActivity(package_name).value_or(""),
                                /*extra_params=*/{});
  }
  return base::StringPrintf("%s;%s;%s=%s;%s=0x%x;%s=%s;%s",
                            app_info.intent.c_str(), kIntentPrefix, kAction,
                            app_info.action.c_str(), kLaunchFlags,
                            arc::Intent::FLAG_ACTIVITY_NEW_TASK |
                                arc::Intent::FLAG_ACTIVITY_RESET_TASK_IF_NEEDED,
                            kPackage, package_name.c_str(), kEndSuffix);
}

std::vector<AndroidAppInfo> GetAppsInfo() {
  std::vector<AndroidAppInfo> android_apps_info;
  auto* prefs = ArcAppListPrefs::Get(ProfileManager::GetActiveUserProfile());
  if (!prefs) {
    LOG(ERROR) << "ArcAppListPrefs is not available.";
    return android_apps_info;
  }
  for (const auto& app_id : prefs->GetAppIds()) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
    if (!app_info)
      continue;
    AndroidAppInfo android_app_info;
    android_app_info.package_name = app_info->package_name;
    auto package = prefs->GetPackage(app_info->package_name);
    if (package)
      android_app_info.version = package->package_version;
    android_app_info.localized_app_name = app_info->name;
    android_app_info.intent = app_info->intent_uri;
    android_apps_info.push_back(std::move(android_app_info));
  }
  return android_apps_info;
}

void NotifyAndroidAppListRefreshed(
    base::ObserverList<ash::assistant::AppListEventSubscriber>* subscribers) {
  std::vector<AndroidAppInfo> android_apps_info = GetAppsInfo();
  for (auto& subscriber : *subscribers)
    subscriber.OnAndroidAppListRefreshed(android_apps_info);
}

}  // namespace

DeviceActions::DeviceActions(std::unique_ptr<DeviceActionsDelegate> delegate)
    : delegate_(std::move(delegate)) {
  ash::GetBluetoothConfigService(
      remote_cros_bluetooth_config_.BindNewPipeAndPassReceiver());
}

DeviceActions::~DeviceActions() = default;

void DeviceActions::SetWifiEnabled(bool enabled) {
  NET_LOG(USER) << __func__ << ":" << enabled;
  NetworkHandler::Get()->technology_state_controller()->SetTechnologiesEnabled(
      NetworkTypePattern::WiFi(), enabled,
      ash::network_handler::ErrorCallback());
}

void DeviceActions::SetBluetoothEnabled(bool enabled) {
  remote_cros_bluetooth_config_->SetBluetoothEnabledState(enabled);
}

void HandleScreenBrightnessCallback(
    DeviceActions::GetScreenBrightnessLevelCallback callback,
    std::optional<double> level) {
  if (level.has_value()) {
    std::move(callback).Run(true, level.value() / 100.0);
  } else {
    std::move(callback).Run(false, 0.0);
  }
}

void DeviceActions::GetScreenBrightnessLevel(
    DeviceActions::GetScreenBrightnessLevelCallback callback) {
  chromeos::PowerManagerClient::Get()->GetScreenBrightnessPercent(
      base::BindOnce(&HandleScreenBrightnessCallback, std::move(callback)));
}

void DeviceActions::SetScreenBrightnessLevel(double level, bool gradual) {
  power_manager::SetBacklightBrightnessRequest request;
  request.set_percent(level * 100);
  request.set_transition(
      gradual
          ? power_manager::SetBacklightBrightnessRequest_Transition_FAST
          : power_manager::SetBacklightBrightnessRequest_Transition_INSTANT);
  request.set_cause(
      power_manager::SetBacklightBrightnessRequest_Cause_USER_REQUEST);
  chromeos::PowerManagerClient::Get()->SetScreenBrightness(request);
}

void DeviceActions::SetNightLightEnabled(bool enabled) {
  const user_manager::User* const user =
      user_manager::UserManager::Get()->GetActiveUser();
  Profile* profile = ash::ProfileHelper::Get()->GetProfileByUser(user);
  DCHECK(profile);
  // Simply toggle the user pref, which is being observed by ash's night
  // light controller.
  profile->GetPrefs()->SetBoolean(ash::prefs::kNightLightEnabled, enabled);
}

void DeviceActions::SetSwitchAccessEnabled(bool enabled) {
  const user_manager::User* const user =
      user_manager::UserManager::Get()->GetActiveUser();
  Profile* profile = ash::ProfileHelper::Get()->GetProfileByUser(user);
  DCHECK(profile);
  profile->GetPrefs()->SetBoolean(ash::prefs::kAccessibilitySwitchAccessEnabled,
                                  enabled);
}

bool DeviceActions::OpenAndroidApp(const AndroidAppInfo& app_info) {
  auto status = delegate_->GetAndroidAppStatus(app_info.package_name);
  if (status != AppStatus::kAvailable)
    return false;

  auto* app = ARC_GET_INSTANCE_FOR_METHOD(
      arc::ArcServiceManager::Get()->arc_bridge_service()->app(),
      LaunchIntentWithWindowInfo);
  if (app) {
    arc::mojom::WindowInfoPtr window_info = arc::mojom::WindowInfo::New();
    window_info->display_id = display::kDefaultDisplayId;
    app->LaunchIntentWithWindowInfo(GetLaunchIntent(std::move(app_info)),
                                    std::move(window_info));
  } else {
    LOG(ERROR) << "Android container is not running. Discard request for launch"
               << app_info.package_name;
  }

  return app != nullptr;
}

AppStatus DeviceActions::GetAndroidAppStatus(const AndroidAppInfo& app_info) {
  return delegate_->GetAndroidAppStatus(app_info.package_name);
}

void DeviceActions::LaunchAndroidIntent(const std::string& intent) {
  auto* app = ARC_GET_INSTANCE_FOR_METHOD(
      arc::ArcServiceManager::Get()->arc_bridge_service()->app(),
      LaunchIntentWithWindowInfo);
  if (!app) {
    LOG(ERROR) << "Android container is not running.";
    return;
  }

  arc::mojom::WindowInfoPtr window_info = arc::mojom::WindowInfo::New();
  window_info->display_id = display::kDefaultDisplayId;
  app->LaunchIntentWithWindowInfo(intent, std::move(window_info));
}

void DeviceActions::AddAndFireAppListEventSubscriber(
    ash::assistant::AppListEventSubscriber* subscriber) {
  auto* prefs = ArcAppListPrefs::Get(ProfileManager::GetActiveUserProfile());
  if (prefs && prefs->package_list_initial_refreshed()) {
    std::vector<AndroidAppInfo> android_apps_info = GetAppsInfo();
    subscriber->OnAndroidAppListRefreshed(android_apps_info);
  }

  app_list_subscribers_.AddObserver(subscriber);

  if (prefs && !scoped_prefs_observations_.IsObservingSource(prefs))
    scoped_prefs_observations_.AddObservation(prefs);
}

void DeviceActions::RemoveAppListEventSubscriber(
    ash::assistant::AppListEventSubscriber* subscriber) {
  app_list_subscribers_.RemoveObserver(subscriber);
}

std::optional<std::string> DeviceActions::GetAndroidAppLaunchIntent(
    const AndroidAppInfo& app_info) {
  auto status = delegate_->GetAndroidAppStatus(app_info.package_name);
  if (status != AppStatus::kAvailable)
    return std::nullopt;

  return GetLaunchIntent(std::move(app_info));
}

void DeviceActions::OnPackageListInitialRefreshed() {
  NotifyAndroidAppListRefreshed(&app_list_subscribers_);
}

void DeviceActions::OnAppRegistered(const std::string& app_id,
                                    const ArcAppListPrefs::AppInfo& app_info) {
  NotifyAndroidAppListRefreshed(&app_list_subscribers_);
}

void DeviceActions::OnAppRemoved(const std::string& id) {
  NotifyAndroidAppListRefreshed(&app_list_subscribers_);
}
