// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/app_access/app_access_notifier.h"

#include <optional>
#include <string>
#include <vector>

#include "app_access_notifier.h"
#include "ash/constants/ash_features.h"
#include "ash/system/privacy/privacy_indicators_controller.h"
#include "ash/system/privacy_hub/camera_privacy_switch_controller.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"

namespace {

apps::AppCapabilityAccessCache* GetAppCapabilityAccessCache(
    AccountId account_id) {
  return apps::AppCapabilityAccessCacheWrapper::Get()
      .GetAppCapabilityAccessCache(account_id);
}

std::optional<std::u16string> MapAppIdToShortName(
    std::string app_id,
    apps::AppCapabilityAccessCache* capability_cache,
    apps::AppRegistryCache* registry_cache,
    const std::set<std::string>& apps_accessing_sensor) {
  DCHECK(capability_cache);
  DCHECK(registry_cache);

  for (const std::string& app : apps_accessing_sensor) {
    std::optional<std::u16string> name;
    registry_cache->ForOneApp(app,
                              [&app_id, &name](const apps::AppUpdate& update) {
                                if (update.AppId() == app_id) {
                                  name = base::UTF8ToUTF16(update.ShortName());
                                }
                              });
    if (name.has_value())
      return name;
  }

  return std::nullopt;
}

// A helper to send `ash::CameraPrivacySwitchController` a notification when an
// application starts or stops using the camera. `application_added` is true
// when the application starts using the camera and false when the application
// stops using the camera.
void SendActiveCameraApplicationsChangedNotification(bool application_added) {
  auto* camera_controller = ash::CameraPrivacySwitchController::Get();
  CHECK(camera_controller);
  camera_controller->ActiveApplicationsChanged(application_added);
}

}  // namespace

AppAccessNotifier::AppAccessNotifier() {
  // These checks are needed for testing, where SessionManager and/or
  // UserManager may not exist.
  session_manager::SessionManager* sm = session_manager::SessionManager::Get();
  if (sm) {
    session_manager_observation_.Observe(sm);
  }
  user_manager::UserManager* um = user_manager::UserManager::Get();
  if (um) {
    user_session_state_observation_.Observe(um);
  }

  CheckActiveUserChanged();
}

AppAccessNotifier::~AppAccessNotifier() = default;

// Returns names of apps accessing camera.
std::vector<std::u16string> AppAccessNotifier::GetAppsAccessingCamera() {
  return GetAppsAccessingSensor(
      &camera_using_app_ids_[active_user_account_id_],
      base::BindOnce([](apps::AppCapabilityAccessCache& cache) {
        return cache.GetAppsAccessingCamera();
      }));
}
// Returns names of apps accessing microphone.
std::vector<std::u16string> AppAccessNotifier::GetAppsAccessingMicrophone() {
  return GetAppsAccessingSensor(
      &mic_using_app_ids_[active_user_account_id_],
      base::BindOnce([](apps::AppCapabilityAccessCache& cache) {
        return cache.GetAppsAccessingMicrophone();
      }));
}

std::vector<std::u16string> AppAccessNotifier::GetAppsAccessingSensor(
    const MruAppIdList* app_id_list,
    base::OnceCallback<std::set<std::string>(apps::AppCapabilityAccessCache&)>
        app_getter) {
  apps::AppRegistryCache* reg_cache = GetActiveUserAppRegistryCache();

  apps::AppCapabilityAccessCache* cap_cache =
      GetActiveUserAppCapabilityAccessCache();

  // A reg_cache and/or cap_cache of value nullptr is possible if we have no
  // active user, e.g. the login screen, so we test and return  empty list in
  // that case instead of using DCHECK().
  if (!reg_cache || !cap_cache || app_id_list->empty()) {
    return {};
  }

  const std::set<std::string>& apps_accessing_sensor =
      std::move(app_getter).Run(*cap_cache);

  std::vector<std::u16string> app_names;
  for (const auto& app_id : *app_id_list) {
    std::optional<std::u16string> app_name = MapAppIdToShortName(
        app_id, cap_cache, reg_cache, apps_accessing_sensor);
    if (app_name.has_value())
      app_names.push_back(app_name.value());
  }
  return app_names;
}

bool AppAccessNotifier::MapContainsAppId(const MruAppIdMap& id_map,
                                         const std::string& app_id) {
  auto it = id_map.find(active_user_account_id_);
  if (it == id_map.end()) {
    return false;
  }
  return base::Contains(it->second, app_id);
}

void AppAccessNotifier::OnCapabilityAccessUpdate(
    const apps::CapabilityAccessUpdate& update) {
  auto app_id = update.AppId();

  const bool is_camera_used = update.Camera().value_or(false);
  const bool is_microphone_used = update.Microphone().value_or(false);

  // TODO(b/261444378): Avoid calculating the booleans and use update.*Changed()
  const bool was_using_camera_already =
      MapContainsAppId(camera_using_app_ids_, app_id);
  const bool was_using_microphone_already =
      MapContainsAppId(mic_using_app_ids_, app_id);

  if (is_camera_used && !was_using_camera_already) {
    // App with id `app_id` started using camera.
    camera_using_app_ids_[active_user_account_id_].push_front(update.AppId());
    SendActiveCameraApplicationsChangedNotification(/*application_added=*/true);
  } else if (!is_camera_used && was_using_camera_already) {
    // App with id `app_id` stopped using camera.
    std::erase(camera_using_app_ids_[active_user_account_id_], update.AppId());
    SendActiveCameraApplicationsChangedNotification(
        /*application_added=*/false);
  }

  if (is_microphone_used && !was_using_microphone_already) {
    // App with id `app_id` started using microphone.
    mic_using_app_ids_[active_user_account_id_].push_front(update.AppId());
  } else if (!is_microphone_used && was_using_microphone_already) {
    // App with id `app_id` stopped using microphone.
    std::erase(mic_using_app_ids_[active_user_account_id_], update.AppId());
  }

  // Privacy indicators is only enabled when Video Conference is disabled.
  if (!ash::features::IsVideoConferenceEnabled()) {
    auto* registry_cache = GetActiveUserAppRegistryCache();
    if (!registry_cache) {
      return;
    }

    auto app_type = registry_cache->GetAppType(app_id);
    std::optional<base::RepeatingClosure> launch_settings_callback;
    if (app_type == apps::AppType::kSystemWeb) {
      // We don't have the capability to launch privacy settings for system web
      // app, so we will disable the settings button for this type of app.
      launch_settings_callback = std::nullopt;
    } else {
      launch_settings_callback =
          base::BindRepeating(&AppAccessNotifier::LaunchAppSettings, app_id);
    }

    ash::PrivacyIndicatorsController::Get()->UpdatePrivacyIndicators(
        app_id, /*app_name=*/GetAppShortNameFromAppId(app_id), is_camera_used,
        is_microphone_used, /*delegate=*/
        base::MakeRefCounted<ash::PrivacyIndicatorsNotificationDelegate>(
            launch_settings_callback),
        ash::PrivacyIndicatorsSource::kApps);

    base::UmaHistogramEnumeration("Ash.PrivacyIndicators.AppAccessUpdate.Type",
                                  registry_cache->GetAppType(app_id));
  }
}

void AppAccessNotifier::OnAppCapabilityAccessCacheWillBeDestroyed(
    apps::AppCapabilityAccessCache* cache) {
  app_capability_access_cache_observation_.Reset();
}

//
// A couple of notes on why we have OnSessionStateChanged() and
// ActiveUserChanged(), i.e. why we observe both SessionManager and UserManager.
//
// The critical logic here is based on knowing when an app starts or stops
// attempting to use the microphone, and for this we observe the active user's
// AppCapabilityAccessCache.  When the active user's AppCapabilityAccessCache
// changes, we need to stop observing any AppCapabilityAccessCache we were
// previously observing and start observing the currently active one.  This is
// the job of CheckActiveUserChanged().
//

void AppAccessNotifier::OnSessionStateChanged() {
  TRACE_EVENT0("ui", "AppAccessNotifier::OnSessionStateChanged");
  session_manager::SessionState state =
      session_manager::SessionManager::Get()->session_state();
  if (state == session_manager::SessionState::ACTIVE) {
    CheckActiveUserChanged();
    session_manager_observation_.Reset();
  }
}

void AppAccessNotifier::ActiveUserChanged(user_manager::User* active_user) {
  CheckActiveUserChanged();
}

// static
std::optional<std::u16string> AppAccessNotifier::GetAppShortNameFromAppId(
    std::string app_id) {
  std::optional<std::u16string> name;
  auto* registry_cache = GetActiveUserAppRegistryCache();
  if (!registry_cache)
    return name;

  registry_cache->ForEachApp([&app_id, &name](const apps::AppUpdate& update) {
    if (update.AppId() == app_id) {
      name = base::UTF8ToUTF16(update.ShortName());
    }
  });
  return name;
}

// static
void AppAccessNotifier::LaunchAppSettings(const std::string& app_id) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile ||
      !apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return;
  }

  auto* registry_cache = GetActiveUserAppRegistryCache();
  if (!registry_cache) {
    return;
  }

  auto app_type = registry_cache->GetAppType(app_id);

  // We don't have the capability to launch privacy settings for system web
  // app, so settings button is disabled for this type of app.
  DCHECK(app_type != apps::AppType::kSystemWeb);

  if (app_type == apps::AppType::kWeb) {
    chrome::ShowAppManagementPage(profile, app_id,
                                  ash::settings::AppManagementEntryPoint::
                                      kPrivacyIndicatorsNotificationSettings);
  } else {
    apps::AppServiceProxyFactory::GetForProfile(profile)->OpenNativeSettings(
        app_id);
  }

  base::UmaHistogramEnumeration("Ash.PrivacyIndicators.LaunchSettings",
                                registry_cache->GetAppType(app_id));
}

AccountId AppAccessNotifier::GetActiveUserAccountId() {
  auto* manager = user_manager::UserManager::Get();
  const user_manager::User* active_user = manager->GetActiveUser();
  if (!active_user)
    return EmptyAccountId();

  return active_user->GetAccountId();
}

void AppAccessNotifier::CheckActiveUserChanged() {
  AccountId id = GetActiveUserAccountId();
  if (id == EmptyAccountId() || id == active_user_account_id_)
    return;

  if (active_user_account_id_ != EmptyAccountId()) {
    app_capability_access_cache_observation_.Reset();
    active_user_account_id_ = EmptyAccountId();
  }

  apps::AppCapabilityAccessCache* cap_cache = GetAppCapabilityAccessCache(id);
  if (cap_cache) {
    app_capability_access_cache_observation_.Observe(cap_cache);
    active_user_account_id_ = id;
  }
}

// static
apps::AppRegistryCache* AppAccessNotifier::GetActiveUserAppRegistryCache() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile ||
      !apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return nullptr;
  }

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  return &proxy->AppRegistryCache();
}

apps::AppCapabilityAccessCache*
AppAccessNotifier::GetActiveUserAppCapabilityAccessCache() {
  return apps::AppCapabilityAccessCacheWrapper::Get()
      .GetAppCapabilityAccessCache(GetActiveUserAccountId());
}
