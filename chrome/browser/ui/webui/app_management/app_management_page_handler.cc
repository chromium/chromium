// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_management/app_management_page_handler.h"

#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/intent_constants.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/preferred_apps_list_handle.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permission_message.h"
#include "extensions/common/permissions/permissions_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/resources/cr_components/app_management/app_management.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/session/connection_holder.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#endif

using apps::mojom::OptionalBool;

namespace {

const char* kAppIdsWithHiddenMoreSettings[] = {
    extensions::kWebStoreAppId,
    extension_misc::kFilesManagerAppId,
};

const char* kAppIdsWithHiddenPinToShelf[] = {
    app_constants::kChromeAppId,
    app_constants::kLacrosAppId,
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char const* kAppIdsWithHiddenStoragePermission[] = {
    arc::kPlayStoreAppId,
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

app_management::mojom::ExtensionAppPermissionMessagePtr
CreateExtensionAppPermissionMessage(
    const extensions::PermissionMessage& message) {
  std::vector<std::string> submessages;
  for (const auto& submessage : message.submessages()) {
    submessages.push_back(base::UTF16ToUTF8(submessage));
  }
  return app_management::mojom::ExtensionAppPermissionMessage::New(
      base::UTF16ToUTF8(message.message()), std::move(submessages));
}

bool ShouldHideMoreSettings(const std::string app_id) {
  return base::Contains(kAppIdsWithHiddenMoreSettings, app_id);
}

bool ShouldHidePinToShelf(const std::string app_id) {
  return base::Contains(kAppIdsWithHiddenPinToShelf, app_id);
}

bool ShouldHideStoragePermission(const std::string app_id) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return base::Contains(kAppIdsWithHiddenStoragePermission, app_id);
#else
  return false;
#endif
}

// Returns a list of intent filters that support http/https given an app ID.
std::vector<apps::mojom::IntentFilterPtr> GetSupportedLinkIntentFilters(
    Profile* profile,
    const std::string& app_id) {
  std::vector<apps::mojom::IntentFilterPtr> intent_filters;
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->AppRegistryCache()
      .ForOneApp(app_id,
                 [&app_id, &intent_filters](const apps::AppUpdate& update) {
                   if (update.Readiness() == apps::mojom::Readiness::kReady) {
                     for (auto& filter : update.IntentFilters()) {
                       if (apps_util::IsSupportedLinkForApp(app_id, filter)) {
                         intent_filters.emplace_back(std::move(filter));
                       }
                     }
                   }
                 });
  return intent_filters;
}

// Returns a list of URLs supported by an app given an app ID.
std::vector<std::string> GetSupportedLinks(Profile* profile,
                                           const std::string& app_id) {
  std::set<std::string> supported_links;
  auto intent_filters = GetSupportedLinkIntentFilters(profile, app_id);
  for (auto& filter : intent_filters) {
    for (const auto& link : apps_util::AppManagementGetSupportedLinks(filter)) {
      supported_links.insert(link);
    }
  }

  return std::vector<std::string>(supported_links.begin(),
                                  supported_links.end());
}

}  // namespace

AppManagementPageHandler::AppManagementPageHandler(
    mojo::PendingReceiver<app_management::mojom::PageHandler> receiver,
    mojo::PendingRemote<app_management::mojom::Page> page,
    Profile* profile,
    Delegate& delegate)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      profile_(profile),
      delegate_(delegate),
#if BUILDFLAG(IS_CHROMEOS_ASH)
      shelf_delegate_(this, profile),
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      preferred_apps_list_handle_(
          apps::AppServiceProxyFactory::GetForProfile(profile)
              ->PreferredApps()) {
  app_registry_cache_observer_.Observe(
      &apps::AppServiceProxyFactory::GetForProfile(profile_)
           ->AppRegistryCache());
  preferred_apps_list_handle_observer_.Observe(&preferred_apps_list_handle_);
}

AppManagementPageHandler::~AppManagementPageHandler() {}

void AppManagementPageHandler::OnPinnedChanged(const std::string& app_id,
                                               bool pinned) {
  app_management::mojom::AppPtr app;

  apps::AppServiceProxyFactory::GetForProfile(profile_)
      ->AppRegistryCache()
      .ForOneApp(app_id, [this, &app](const apps::AppUpdate& update) {
        if (update.Readiness() == apps::mojom::Readiness::kReady)
          app = CreateUIAppPtr(update);
      });

  // If an app with this id is not already installed, do nothing.
  if (!app)
    return;

  app->is_pinned = pinned ? OptionalBool::kTrue : OptionalBool::kFalse;

  page_->OnAppChanged(std::move(app));
}

void AppManagementPageHandler::GetApps(GetAppsCallback callback) {
  std::vector<app_management::mojom::AppPtr> apps;
  apps::AppServiceProxyFactory::GetForProfile(profile_)
      ->AppRegistryCache()
      .ForEachApp([this, &apps](const apps::AppUpdate& update) {
        if (update.ShowInManagement() == apps::mojom::OptionalBool::kTrue &&
            apps_util::IsInstalled(update.Readiness())) {
          apps.push_back(CreateUIAppPtr(update));
        }
      });

  std::move(callback).Run(std::move(apps));
}

void AppManagementPageHandler::GetApp(const std::string& app_id,
                                      GetAppCallback callback) {
  app_management::mojom::AppPtr app;

  apps::AppServiceProxyFactory::GetForProfile(profile_)
      ->AppRegistryCache()
      .ForOneApp(app_id, [this, &app](const apps::AppUpdate& update) {
        if (update.Readiness() == apps::mojom::Readiness::kReady)
          app = CreateUIAppPtr(update);
      });

  std::move(callback).Run(std::move(app));
}

void AppManagementPageHandler::GetExtensionAppPermissionMessages(
    const std::string& app_id,
    GetExtensionAppPermissionMessagesCallback callback) {
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile_);
  const extensions::Extension* extension = registry->GetExtensionById(
      app_id, extensions::ExtensionRegistry::ENABLED |
                  extensions::ExtensionRegistry::DISABLED |
                  extensions::ExtensionRegistry::BLOCKLISTED);
  std::vector<app_management::mojom::ExtensionAppPermissionMessagePtr> messages;
  if (extension) {
    for (const auto& message :
         extension->permissions_data()->GetPermissionMessages()) {
      messages.push_back(CreateExtensionAppPermissionMessage(message));
    }
  }
  std::move(callback).Run(std::move(messages));
}

void AppManagementPageHandler::SetPinned(const std::string& app_id,
                                         OptionalBool pinned) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  shelf_delegate_.SetPinned(app_id, pinned);
#else
  NOTREACHED();
#endif
}

void AppManagementPageHandler::SetPermission(
    const std::string& app_id,
    apps::mojom::PermissionPtr permission) {
  apps::AppServiceProxyFactory::GetForProfile(profile_)->SetPermission(
      app_id, std::move(permission));
}

void AppManagementPageHandler::SetResizeLocked(const std::string& app_id,
                                               bool locked) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  apps::AppServiceProxyFactory::GetForProfile(profile_)->SetResizeLocked(
      app_id, locked ? OptionalBool::kTrue : OptionalBool::kFalse);
#else
  NOTREACHED();
#endif
}

void AppManagementPageHandler::Uninstall(const std::string& app_id) {
  apps::AppServiceProxyFactory::GetForProfile(profile_)->Uninstall(
      app_id, apps::mojom::UninstallSource::kAppManagement,
      delegate_.GetUninstallAnchorWindow());
}

void AppManagementPageHandler::OpenNativeSettings(const std::string& app_id) {
  apps::AppServiceProxyFactory::GetForProfile(profile_)->OpenNativeSettings(
      app_id);
}

void AppManagementPageHandler::SetPreferredApp(const std::string& app_id,
                                               bool is_preferred_app) {
  bool is_preferred_app_for_supported_links =
      preferred_apps_list_handle_.IsPreferredAppForSupportedLinks(app_id);
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);

  if (is_preferred_app && !is_preferred_app_for_supported_links) {
    proxy->SetSupportedLinksPreference(app_id);
  } else if (!is_preferred_app && is_preferred_app_for_supported_links) {
    proxy->RemoveSupportedLinksPreference(app_id);
  }
}

void AppManagementPageHandler::GetOverlappingPreferredApps(
    const std::string& app_id,
    GetOverlappingPreferredAppsCallback callback) {
  auto intent_filters = GetSupportedLinkIntentFilters(profile_, app_id);
  base::flat_set<std::string> app_ids =
      preferred_apps_list_handle_.FindPreferredAppsForFilters(intent_filters);
  app_ids.erase(app_id);
  // Remove the use_browser app ID as it's mainly used inside the intent system and is not an app
  // in app management. This prevents an overlap dialog from being shown when there are no "real"
  // apps that overlap.
  app_ids.erase(apps::kUseBrowserForLink);
  std::move(callback).Run(std::move(app_ids).extract());
}

void AppManagementPageHandler::SetWindowMode(
    const std::string& app_id,
    apps::mojom::WindowMode window_mode) {
  // On ChromeOS, apps should always open in a new window,
  // hence window mode changes are not allowed.
#if BUILDFLAG(IS_CHROMEOS)
  NOTREACHED();
#else
  apps::AppServiceProxyFactory::GetForProfile(profile_)->SetWindowMode(
      app_id, window_mode);
#endif
}

void AppManagementPageHandler::SetRunOnOsLoginMode(
    const std::string& app_id,
    apps::mojom::RunOnOsLoginMode run_on_os_login_mode) {
#if BUILDFLAG(IS_CHROMEOS)
  NOTREACHED();
#else
  apps::AppServiceProxyFactory::GetForProfile(profile_)->SetRunOnOsLoginMode(
      app_id, run_on_os_login_mode);
#endif
}

app_management::mojom::AppPtr AppManagementPageHandler::CreateUIAppPtr(
    const apps::AppUpdate& update) {
  base::flat_map<apps::mojom::PermissionType, apps::mojom::PermissionPtr>
      permissions;
  for (const auto& permission : update.Permissions()) {
    if (permission->permission_type == apps::mojom::PermissionType::kStorage &&
        ShouldHideStoragePermission(update.AppId())) {
      continue;
    }
    permissions[permission->permission_type] = permission->Clone();
  }

  auto app = app_management::mojom::App::New();
  app->id = update.AppId();
  app->type = update.AppType();
  app->title = update.Name();
  app->permissions = std::move(permissions);
  app->install_reason = update.InstallReason();

  app->description = update.Description();

  // On other OS's, is_pinned defaults to OptionalBool::kUnknown, which is
  // used to represent the fact that there is no concept of being pinned.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  app->is_pinned = shelf_delegate_.IsPinned(update.AppId())
                       ? OptionalBool::kTrue
                       : OptionalBool::kFalse;
  app->is_policy_pinned = shelf_delegate_.IsPolicyPinned(update.AppId())
                              ? OptionalBool::kTrue
                              : OptionalBool::kFalse;
  app->resize_locked = update.ResizeLocked() == OptionalBool::kTrue;
  app->hide_resize_locked =
      update.ResizeLocked() == apps::mojom::OptionalBool::kUnknown;
#endif
  app->is_preferred_app =
      preferred_apps_list_handle_.IsPreferredAppForSupportedLinks(
          update.AppId());
  app->hide_more_settings = ShouldHideMoreSettings(app->id);
  app->hide_pin_to_shelf =
      update.ShowInShelf() == apps::mojom::OptionalBool::kFalse ||
      ShouldHidePinToShelf(app->id);
  app->window_mode = update.WindowMode();
  app->supported_links = GetSupportedLinks(profile_, app->id);
  app->run_on_os_login = update.RunOnOsLogin();

  return app;
}

void AppManagementPageHandler::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.ShowInManagementChanged() || update.ReadinessChanged()) {
    if (update.ShowInManagement() == apps::mojom::OptionalBool::kTrue &&
        update.Readiness() == apps::mojom::Readiness::kReady) {
      page_->OnAppAdded(CreateUIAppPtr(update));
    }

    if (update.ShowInManagement() == apps::mojom::OptionalBool::kFalse ||
        !apps_util::IsInstalled(update.Readiness())) {
      page_->OnAppRemoved(update.AppId());
    }
  } else {
    page_->OnAppChanged(CreateUIAppPtr(update));
  }
}

void AppManagementPageHandler::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  cache->RemoveObserver(this);
}

void AppManagementPageHandler::OnPreferredAppChanged(const std::string& app_id,
                                                     bool is_preferred_app) {
  app_management::mojom::AppPtr app;

  apps::AppServiceProxyFactory::GetForProfile(profile_)
      ->AppRegistryCache()
      .ForOneApp(app_id, [this, &app](const apps::AppUpdate& update) {
        if (update.Readiness() == apps::mojom::Readiness::kReady)
          app = CreateUIAppPtr(update);
      });

  // If an app with this id is not already installed, do nothing.
  if (!app)
    return;

  app->is_preferred_app = is_preferred_app;

  page_->OnAppChanged(std::move(app));
}

void AppManagementPageHandler::OnPreferredAppsListWillBeDestroyed(
    apps::PreferredAppsListHandle* handle) {
  handle->RemoveObserver(this);
}
