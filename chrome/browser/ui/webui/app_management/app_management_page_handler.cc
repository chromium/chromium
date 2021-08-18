// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_management/app_management_page_handler.h"

#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/app_management/app_management.mojom.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/preferred_apps_list.h"
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "components/arc/session/connection_holder.h"
#endif

using apps::mojom::OptionalBool;

namespace {

constexpr char const* kAppIdsWithHiddenMoreSettings[] = {
    extensions::kWebStoreAppId,
    extension_misc::kFilesManagerAppId,
};

constexpr char const* kAppIdsWithHiddenPinToShelf[] = {
    extension_misc::kChromeAppId,
    extension_misc::kLacrosAppId,
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

}  // namespace

AppManagementPageHandler::AppManagementPageHandler(
    mojo::PendingReceiver<app_management::mojom::PageHandler> receiver,
    mojo::PendingRemote<app_management::mojom::Page> page,
    Profile* profile)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      profile_(profile)
#if BUILDFLAG(IS_CHROMEOS_ASH)
      ,
      shelf_delegate_(this, profile)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      ,
      preferred_apps_list_(apps::AppServiceProxyFactory::GetForProfile(profile)
                               ->PreferredApps()) {
  app_registry_cache_observer_.Observe(
      &apps::AppServiceProxyFactory::GetForProfile(profile_)
           ->AppRegistryCache());
  preferred_apps_list_observer_.Observe(&preferred_apps_list_);
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
      nullptr /* parent_window */);
}

void AppManagementPageHandler::OpenNativeSettings(const std::string& app_id) {
  apps::AppServiceProxyFactory::GetForProfile(profile_)->OpenNativeSettings(
      app_id);
}

void AppManagementPageHandler::SetPreferredApp(const std::string& app_id,
                                               bool is_preferred_app) {
  if (is_preferred_app &&
      !preferred_apps_list_.IsPreferredAppForSupportedLinks(app_id)) {
    // Only deal with overlapping links if we actually changed the permission
    // to true.
    apps::AppServiceProxyFactory::GetForProfile(profile_)
        ->AppRegistryCache()
        .ForOneApp(app_id, [this](const apps::AppUpdate& update) {
          if (update.Readiness() == apps::mojom::Readiness::kReady) {
            for (auto& filter : update.IntentFilters()) {
              if (apps_util::IsSupportedLink(filter)) {
                this->preferred_apps_list_.AddPreferredApp(update.AppId(),
                                                           filter);
              }
            }
          }
        });
  } else if (!is_preferred_app &&
             preferred_apps_list_.IsPreferredAppForSupportedLinks(app_id)) {
    // If changed to false, remove all of the filters for that app.
    // Only deal with overlapping links if we actually changed the permission
    // to true.
    apps::AppServiceProxyFactory::GetForProfile(profile_)
        ->AppRegistryCache()
        .ForOneApp(app_id, [this](const apps::AppUpdate& update) {
          if (update.Readiness() == apps::mojom::Readiness::kReady) {
            for (auto& filter : update.IntentFilters()) {
              if (apps_util::IsSupportedLink(filter)) {
                this->preferred_apps_list_.DeletePreferredApp(update.AppId(),
                                                              filter);
              }
            }
          }
        });
  }
}

app_management::mojom::AppPtr AppManagementPageHandler::CreateUIAppPtr(
    const apps::AppUpdate& update) {
  base::flat_map<uint32_t, apps::mojom::PermissionPtr> permissions;
  for (const auto& permission : update.Permissions()) {
    if (static_cast<app_management::mojom::ArcPermissionType>(
            permission->permission_id) ==
            app_management::mojom::ArcPermissionType::STORAGE &&
        ShouldHideStoragePermission(update.AppId())) {
      continue;
    }
    permissions[permission->permission_id] = permission->Clone();
  }

  auto app = app_management::mojom::App::New();
  app->id = update.AppId();
  app->type = update.AppType();
  app->title = update.Name();
  app->permissions = std::move(permissions);
  app->install_source = update.InstallSource();

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
      preferred_apps_list_.IsPreferredAppForSupportedLinks(update.AppId());
  app->hide_more_settings = ShouldHideMoreSettings(app->id);
  app->hide_pin_to_shelf =
      update.ShowInShelf() == apps::mojom::OptionalBool::kFalse ||
      ShouldHidePinToShelf(app->id);
  app->window_mode = update.WindowMode();
  app->supported_links = GetSupportedLinksList(app->id);

  return app;
}

std::vector<std::string> AppManagementPageHandler::GetSupportedLinksList(
    const std::string& app_id) {
  std::vector<std::string> links;
  apps::AppServiceProxyFactory::GetForProfile(profile_)
      ->AppRegistryCache()
      .ForOneApp(app_id, [&links](const apps::AppUpdate& update) {
        if (update.Readiness() == apps::mojom::Readiness::kReady) {
          std::set<std::string> seen;
          for (const auto& filter : update.IntentFilters()) {
            if (apps_util::IsSupportedLink(filter)) {
              for (const auto& link :
                   apps_util::AppManagementGetSupportedLinks(filter)) {
                // Add link to list if it hasn't already been seen.
                if (seen.find(link) == seen.end()) {
                  links.emplace_back(link);
                  seen.insert(link);
                }
              }
            }
          }
        }
      });
  return links;
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
    apps::PreferredAppsList* list) {
  list->RemoveObserver(this);
}
