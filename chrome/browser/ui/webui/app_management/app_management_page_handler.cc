// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_management/app_management_page_handler.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/debug/dump_without_crashing.h"
#include "base/i18n/message_formatter.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_prefs_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registrar_observer.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "components/services/app_service/public/cpp/preferred_apps_list_handle.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permission_message.h"
#include "extensions/common/permissions/permissions_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/events/event_constants.h"
#include "ui/webui/resources/cr_components/app_management/app_management.mojom.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/session/connection_holder.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/apps/apk_web_app_service.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/web_app_service_ash.h"
#include "chromeos/crosapi/mojom/web_app_service.mojom.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/default_apps_util.h"
#endif

using app_management::mojom::OptionalBool;

namespace {

const char* kAppIdsWithHiddenMoreSettings[] = {
    extensions::kWebStoreAppId,
    extension_misc::kFilesManagerAppId,
};

const char* kAppIdsWithHiddenPinToShelf[] = {
    app_constants::kChromeAppId,
    app_constants::kLacrosAppId,
};

const char kFileHandlingLearnMore[] =
    "https://support.google.com/chrome/?p=pwa_default_associations";

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

// Returns true if Chrome can direct users to a centralized system UI for
// setting default apps/file type associations. If false, a "Learn More" link
// will be shown instead.
bool CanShowDefaultAppAssociationsUi() {
#if BUILDFLAG(IS_WIN)
  return true;
#else
  return false;
#endif
}

// Returns a list of intent filters that support http/https given an app ID.
apps::IntentFilters GetSupportedLinkIntentFilters(Profile* profile,
                                                  const std::string& app_id) {
  apps::IntentFilters intent_filters;
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->AppRegistryCache()
      .ForOneApp(app_id,
                 [&app_id, &intent_filters](const apps::AppUpdate& update) {
                   if (update.Readiness() == apps::Readiness::kReady) {
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
    for (const auto& link :
         apps_util::GetSupportedLinksForAppManagement(filter)) {
      supported_links.insert(link);
    }
  }

  return std::vector<std::string>(supported_links.begin(),
                                  supported_links.end());
}

#if !BUILDFLAG(IS_CHROMEOS)
std::vector<std::string> GetSupportedLinksForPWAs(
    const std::string& app_id,
    web_app::WebAppProvider& provider) {
  GURL app_scope = provider.registrar_unsafe().GetAppScope(app_id);
  if (!web_app::IsValidScopeForLinkCapturing(app_scope)) {
    return std::vector<std::string>();
  }

  std::string scope_str(app_scope.host());
  if (app_scope.has_port()) {
    scope_str += ":" + app_scope.port();
  }
  scope_str += app_scope.path();
  if (scope_str.back() == '/') {
    scope_str = scope_str + "*";
  } else {
    scope_str = scope_str + "/*";
  }
  return {scope_str};
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

absl::optional<std::string> MaybeFormatBytes(absl::optional<uint64_t> bytes) {
  if (bytes.has_value()) {
    // ui::FormatBytes requires a non-negative signed integer. In general, we
    // expect that converting from unsigned to signed int here should always
    // yield a positive value, since overflowing into negative would require an
    // implausibly large app (2^63 bytes ~= 9 exabytes).
    int64_t signed_bytes = static_cast<int64_t>(bytes.value());
    if (signed_bytes < 0) {
      // TODO(crbug.com/1418590): Investigate ARC apps which have negative data
      // sizes.
      LOG(ERROR) << "Invalid app size: " << signed_bytes;
      base::debug::DumpWithoutCrashing();
      return absl::nullopt;
    }
    return base::UTF16ToUTF8(ui::FormatBytes(signed_bytes));
  }

  return absl::nullopt;
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
      shelf_delegate_(this, profile),
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      delegate_(delegate) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile_);
  app_registry_cache_observer_.Observe(&proxy->AppRegistryCache());
  preferred_apps_list_handle_observer_.Observe(&proxy->PreferredAppsList());

  // On Chrome OS, file handler updates are already plumbed through
  // App Service since the change will also affect the intent filters.
  // There's no need to update twice.
#if !BUILDFLAG(IS_CHROMEOS)
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile_);
  registrar_observation_.Observe(&provider->registrar_unsafe());
#endif
}

AppManagementPageHandler::~AppManagementPageHandler() {}

void AppManagementPageHandler::OnPinnedChanged(const std::string& app_id,
                                               bool pinned) {
  app_management::mojom::AppPtr app;

  apps::AppServiceProxyFactory::GetForProfile(profile_)
      ->AppRegistryCache()
      .ForOneApp(app_id, [this, &app](const apps::AppUpdate& update) {
        if (update.Readiness() == apps::Readiness::kReady) {
          app = CreateUIAppPtr(update);
        }
      });

  // If an app with this id is not already installed, do nothing.
  if (!app) {
    return;
  }

  app->is_pinned = pinned ? OptionalBool::kTrue : OptionalBool::kFalse;

  page_->OnAppChanged(std::move(app));
}

void AppManagementPageHandler::GetApps(GetAppsCallback callback) {
  std::vector<app_management::mojom::AppPtr> app_management_apps;
  std::vector<apps::AppPtr> apps =
      apps::AppServiceProxyFactory::GetForProfile(profile_)
          ->AppRegistryCache()
          .GetAllApps();

  // TODO(crbug/1315958): A speculative fix for crbug/1315958. If the crashes go
  // away, there is some other bugs in CreateUIAppPtr, and we need to track down
  // here so that the ForEachApps call works.
  for (auto& app : apps) {
    if (app->show_in_management.value_or(false) &&
        apps_util::IsInstalled(app->readiness)) {
      app_management_apps.push_back(
          CreateUIAppPtr(apps::AppUpdate(app.get(),
                                         /*delta=*/nullptr, AccountId())));
    }
  }

  std::move(callback).Run(std::move(app_management_apps));
}

void AppManagementPageHandler::GetApp(const std::string& app_id,
                                      GetAppCallback callback) {
  app_management::mojom::AppPtr app;

  apps::AppServiceProxyFactory::GetForProfile(profile_)
      ->AppRegistryCache()
      .ForOneApp(app_id, [this, &app](const apps::AppUpdate& update) {
        if (update.Readiness() == apps::Readiness::kReady) {
          app = CreateUIAppPtr(update);
        }
      });

  std::move(callback).Run(std::move(app));
}

void AppManagementPageHandler::GetSubAppToParentMap(
    GetSubAppToParentMapCallback callback) {
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile_);
  if (provider) {
    // Web apps are managed in the current process (Ash or Lacros).
    provider->scheduler().ScheduleCallbackWithLock<web_app::AllAppsLock>(
        "AppManagementPageHandler::GetSubAppToParentMap",
        std::make_unique<web_app::AllAppsLockDescription>(),
        base::BindOnce([](web_app::AllAppsLock& lock) {
          return lock.registrar().GetSubAppToParentMap();
        }).Then(std::move(callback)));
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Web app data needs to be fetched from the Lacros process.
  crosapi::mojom::WebAppProviderBridge* web_app_provider_bridge =
      crosapi::CrosapiManager::Get()
          ->crosapi_ash()
          ->web_app_service_ash()
          ->GetWebAppProviderBridge();
  if (web_app_provider_bridge) {
    web_app_provider_bridge->GetSubAppToParentMap(std::move(callback));
    return;
  }
  LOG(ERROR) << "Could not find WebAppProviderBridge.";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Reaching here means that WebAppProviderBridge and WebAppProvider were both
  // not found.
  std::move(callback).Run(base::flat_map<std::string, std::string>());
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

void AppManagementPageHandler::SetPermission(const std::string& app_id,
                                             apps::PermissionPtr permission) {
  apps::AppServiceProxyFactory::GetForProfile(profile_)->SetPermission(
      app_id, std::move(permission));
}

void AppManagementPageHandler::SetResizeLocked(const std::string& app_id,
                                               bool locked) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  apps::AppServiceProxyFactory::GetForProfile(profile_)->SetResizeLocked(
      app_id, locked);
#else
  NOTREACHED();
#endif
}

void AppManagementPageHandler::Uninstall(const std::string& app_id) {
  apps::AppServiceProxyFactory::GetForProfile(profile_)->Uninstall(
      app_id, apps::UninstallSource::kAppManagement,
      delegate_->GetUninstallAnchorWindow());
}

void AppManagementPageHandler::OpenNativeSettings(const std::string& app_id) {
  apps::AppServiceProxyFactory::GetForProfile(profile_)->OpenNativeSettings(
      app_id);
}

void AppManagementPageHandler::SetPreferredApp(const std::string& app_id,
                                               bool is_preferred_app) {
#if BUILDFLAG(IS_CHROMEOS)
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
  bool is_preferred_app_for_supported_links =
      proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_id);

  if (is_preferred_app && !is_preferred_app_for_supported_links) {
    proxy->SetSupportedLinksPreference(app_id);
  } else if (!is_preferred_app && is_preferred_app_for_supported_links) {
    proxy->RemoveSupportedLinksPreference(app_id);
  }
#else
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(profile_);
  provider->scheduler().ScheduleCallbackWithLock<web_app::AllAppsLock>(
      "AppManagementPageHandler::MakeAppPreferredAndResetOthers",
      std::make_unique<web_app::AllAppsLockDescription>(),
      base::BindOnce(&AppManagementPageHandler::MakeAppPreferredAndResetOthers,
                     weak_ptr_factory_.GetWeakPtr(), app_id, is_preferred_app));
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void AppManagementPageHandler::GetOverlappingPreferredApps(
    const std::string& app_id,
    GetOverlappingPreferredAppsCallback callback) {
#if BUILDFLAG(IS_CHROMEOS)
  auto intent_filters = GetSupportedLinkIntentFilters(profile_, app_id);
  base::flat_set<std::string> app_ids =
      apps::AppServiceProxyFactory::GetForProfile(profile_)
          ->PreferredAppsList()
          .FindPreferredAppsForFilters(intent_filters);
  app_ids.erase(app_id);
  // Remove the use_browser app ID as it's mainly used inside the intent system
  // and is not an app in app management. This prevents an overlap dialog from
  // being shown when there are no "real" apps that overlap.
  app_ids.erase(apps_util::kUseBrowserForLink);
  std::move(callback).Run(std::move(app_ids).extract());
#else
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(profile_);
  provider->scheduler().ScheduleCallbackWithLock<web_app::AllAppsLock>(
      "AppManagementPageHandler::GetOverlappingPreferredApps",
      std::make_unique<web_app::AllAppsLockDescription>(),
      base::BindOnce(
          [](const webapps::AppId& app_id,
             GetOverlappingPreferredAppsCallback callback,
             web_app::AllAppsLock& all_apps_lock) {
            std::move(callback).Run(
                all_apps_lock.registrar().GetOverlappingAppsMatchingScope(
                    app_id));
          },
          app_id, std::move(callback)));
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void AppManagementPageHandler::UpdateAppSize(const std::string& app_id) {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
  proxy->UpdateAppSize(app_id);
}

void AppManagementPageHandler::SetWindowMode(const std::string& app_id,
                                             apps::WindowMode window_mode) {
  // On ChromeOS, apps should always open in a new window,
  // hence window mode changes are not allowed.
#if BUILDFLAG(IS_CHROMEOS)
  NOTREACHED();
#else
  auto* provider = web_app::WebAppProvider::GetForLocalAppsUnchecked(profile_);

  // Changing window mode is not allowed for isolated web apps.
  if (provider->registrar_unsafe().IsIsolated(app_id)) {
    NOTREACHED();
  } else {
    apps::AppServiceProxyFactory::GetForProfile(profile_)->SetWindowMode(
        app_id, window_mode);
  }
#endif
}

void AppManagementPageHandler::SetRunOnOsLoginMode(
    const std::string& app_id,
    apps::RunOnOsLoginMode run_on_os_login_mode) {
#if BUILDFLAG(IS_CHROMEOS)
  NOTREACHED();
#else
  apps::AppServiceProxyFactory::GetForProfile(profile_)->SetRunOnOsLoginMode(
      app_id, run_on_os_login_mode);
#endif
}

void AppManagementPageHandler::SetFileHandlingEnabled(const std::string& app_id,
                                                      bool enabled) {
  auto permission = std::make_unique<apps::Permission>(
      apps::PermissionType::kFileHandling, enabled,
      /*is_managed=*/false);
  apps::AppServiceProxyFactory::GetForProfile(profile_)->SetPermission(
      app_id, std::move(permission));
}

void AppManagementPageHandler::ShowDefaultAppAssociationsUi() {
  DCHECK(CanShowDefaultAppAssociationsUi());
#if BUILDFLAG(IS_WIN)
  base::win::LaunchDefaultAppsSettingsModernDialog({});
#endif
}

void AppManagementPageHandler::OnWebAppFileHandlerApprovalStateChanged(
    const webapps::AppId& app_id) {
#if BUILDFLAG(IS_CHROMEOS)
  NOTREACHED();
#endif
  app_management::mojom::AppPtr app;

  apps::AppServiceProxyFactory::GetForProfile(profile_)
      ->AppRegistryCache()
      .ForOneApp(app_id, [this, &app](const apps::AppUpdate& update) {
        if (update.Readiness() == apps::Readiness::kReady) {
          app = CreateUIAppPtr(update);
        }
      });

  if (!app) {
    return;
  }

  page_->OnAppChanged(std::move(app));
}

void AppManagementPageHandler::OnAppRegistrarDestroyed() {
  registrar_observation_.Reset();
}

#if !BUILDFLAG(IS_CHROMEOS)
void AppManagementPageHandler::OnWebAppUserLinkCapturingPreferencesChanged(
    const webapps::AppId& app_id,
    bool is_preferred) {
  OnPreferredAppChanged(app_id, is_preferred);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

app_management::mojom::AppPtr AppManagementPageHandler::CreateUIAppPtr(
    const apps::AppUpdate& update) {
  auto app = app_management::mojom::App::New();
  app->id = update.AppId();
  app->type = update.AppType();
  app->title = update.Name();

  for (const auto& permission : update.Permissions()) {
    if (permission->permission_type == apps::PermissionType::kStorage &&
        ShouldHideStoragePermission(update.AppId())) {
      continue;
    }
    app->permissions[permission->permission_type] = permission->Clone();
  }

  app->install_reason = update.InstallReason();
  app->install_source = update.InstallSource();

  app->version = update.Version();

  app->description = update.Description();

  app->app_size = MaybeFormatBytes(update.AppSizeInBytes());
  app->data_size = MaybeFormatBytes(update.DataSizeInBytes());

  // On other OS's, is_pinned defaults to OptionalBool::kUnknown, which is
  // used to represent the fact that there is no concept of being pinned.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  app->is_pinned = shelf_delegate_.IsPinned(update.AppId())
                       ? OptionalBool::kTrue
                       : OptionalBool::kFalse;
  app->is_policy_pinned = shelf_delegate_.IsPolicyPinned(update.AppId())
                              ? OptionalBool::kTrue
                              : OptionalBool::kFalse;
  app->resize_locked = update.ResizeLocked().value_or(false);
  app->hide_resize_locked = !update.ResizeLocked().has_value();
#endif
#if BUILDFLAG(IS_CHROMEOS)
  app->is_preferred_app = apps::AppServiceProxyFactory::GetForProfile(profile_)
                              ->PreferredAppsList()
                              .IsPreferredAppForSupportedLinks(update.AppId());
#else
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(profile_);
  CHECK(provider);
  app->is_preferred_app =
      provider->registrar_unsafe().CapturesLinksInScope(update.AppId());
#endif  // BUILDFLAG(IS_CHROMEOS)
  app->hide_more_settings = ShouldHideMoreSettings(app->id);
  app->hide_pin_to_shelf =
      !update.ShowInShelf().value_or(true) || ShouldHidePinToShelf(app->id);
  app->window_mode = update.WindowMode();

#if BUILDFLAG(IS_CHROMEOS)
  app->supported_links = GetSupportedLinks(profile_, app->id);
#else
  // This allows us to bypass showing the supported links item on the PWA app
  // settings page on Windows, Mac and Linux platforms.
  if (base::FeatureList::IsEnabled(features::kDesktopPWAsLinkCapturing)) {
    app->supported_links = GetSupportedLinksForPWAs(app->id, *provider);
  } else {
    app->supported_links = std::vector<std::string>();
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
  auto run_on_os_login = update.RunOnOsLogin();
  if (run_on_os_login.has_value()) {
    app->run_on_os_login = std::make_unique<apps::RunOnOsLogin>(
        std::move(run_on_os_login.value()));
  }

  if (update.AppType() == apps::AppType::kWeb ||
      update.AppType() == apps::AppType::kSystemWeb) {
    std::string file_handling_types;
    std::string file_handling_types_label;
    bool fh_enabled = false;
    const bool is_system_web_app =
        update.InstallReason() == apps::InstallReason::kSystem;
    if (!is_system_web_app &&
        base::FeatureList::IsEnabled(blink::features::kFileHandlingAPI)) {
      apps::IntentFilters filters = update.IntentFilters();
      if (!filters.empty()) {
        std::set<std::string> file_extensions;
        // Mime types are ignored.
        std::set<std::string> mime_types;
        for (auto& filter : filters) {
          bool is_potential_file_handler_action = base::ranges::any_of(
              filter->conditions.begin(), filter->conditions.end(),
              [](const std::unique_ptr<apps::Condition>& condition) {
                if (condition->condition_type != apps::ConditionType::kAction) {
                  return false;
                }

                if (condition->condition_values.size() != 1U) {
                  return false;
                }

                return condition->condition_values[0]->value ==
                       apps_util::kIntentActionPotentialFileHandler;
              });
          if (is_potential_file_handler_action) {
            filter->GetMimeTypesAndExtensions(mime_types, file_extensions);
            break;
          }
        }

        for (const auto& permission : update.Permissions()) {
          if (permission->permission_type ==
              apps::PermissionType::kFileHandling) {
            fh_enabled = permission->IsPermissionEnabled();
            break;
          }
        }

        std::vector<std::u16string> extensions_for_display =
            web_app::TransformFileExtensionsForDisplay(file_extensions);
        file_handling_types = base::UTF16ToUTF8(
            base::JoinString(extensions_for_display,
                             l10n_util::GetStringUTF16(
                                 IDS_WEB_APP_FILE_HANDLING_LIST_SEPARATOR)));

        std::vector<std::u16string> truncated_extensions =
            extensions_for_display;
        // Only show at most 4 extensions.
        truncated_extensions.resize(4);
        file_handling_types_label =
            base::UTF16ToUTF8(base::i18n::MessageFormatter::FormatWithNamedArgs(
                l10n_util::GetStringUTF16(
                    IDS_APP_MANAGEMENT_FILE_HANDLING_TYPES),
                "FILE_TYPE_COUNT",
                static_cast<int>(extensions_for_display.size()), "FILE_TYPE1",
                truncated_extensions[0], "FILE_TYPE2", truncated_extensions[1],
                "FILE_TYPE3", truncated_extensions[2], "FILE_TYPE4",
                truncated_extensions[3], "OVERFLOW_COUNT",
                static_cast<int>(extensions_for_display.size()) -
                    static_cast<int>(truncated_extensions.size()),
                "LINK", "#"));
      }

      absl::optional<GURL> learn_more_url;
      if (!CanShowDefaultAppAssociationsUi()) {
        learn_more_url = GURL(kFileHandlingLearnMore);
      }
      // TODO(crbug/1252505): add file handling policy support.
      app->file_handling_state = app_management::mojom::FileHandlingState::New(
          fh_enabled, /*is_managed=*/false, file_handling_types,
          file_handling_types_label, learn_more_url);
    }
  }

#if !BUILDFLAG(IS_CHROMEOS)
  app->hide_window_mode = provider->registrar_unsafe().IsIsolated(app->id);
#endif

  app->publisher_id = update.PublisherId();

  return app;
}

void AppManagementPageHandler::OpenStorePage(const std::string& app_id) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
  auto* apk_service = ash::ApkWebAppService::Get(profile_);
  proxy->AppRegistryCache().ForOneApp(
      app_id, [&proxy, &apk_service](const apps::AppUpdate& update) {
        if (update.InstallSource() == apps::InstallSource::kPlayStore) {
          std::string package_name = update.PublisherId();
          if (apk_service->IsWebAppInstalledFromArc(update.AppId())) {
            package_name =
                apk_service->GetPackageNameForWebApp(update.AppId()).value();
          }
          GURL url("https://play.google.com/store/apps/details?id=" +
                   package_name);
          proxy->LaunchAppWithUrl(arc::kPlayStoreAppId, ui::EF_NONE, url,
                                  apps::LaunchSource::kFromChromeInternal);
        } else if (update.InstallSource() ==
                   apps::InstallSource::kChromeWebStore) {
          GURL url("https://chrome.google.com/webstore/detail/" +
                   update.AppId());
          proxy->LaunchAppWithUrl(extensions::kWebStoreAppId, ui::EF_NONE, url,
                                  apps::LaunchSource::kFromChromeInternal);
        }
      });
#endif
}

void AppManagementPageHandler::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.ShowInManagementChanged() || update.ReadinessChanged()) {
    if (update.ShowInManagement().value_or(false) &&
        update.Readiness() == apps::Readiness::kReady) {
      page_->OnAppAdded(CreateUIAppPtr(update));
    }

    if (!update.ShowInManagement().value_or(true) ||
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
        if (update.Readiness() == apps::Readiness::kReady) {
          app = CreateUIAppPtr(update);
        }
      });

  // If an app with this id is not already installed, do nothing.
  if (!app) {
    return;
  }

  app->is_preferred_app = is_preferred_app;

  page_->OnAppChanged(std::move(app));
}

void AppManagementPageHandler::OnPreferredAppsListWillBeDestroyed(
    apps::PreferredAppsListHandle* handle) {
  preferred_apps_list_handle_observer_.Reset();
}

#if !BUILDFLAG(IS_CHROMEOS)
void AppManagementPageHandler::MakeAppPreferredAndResetOthers(
    const webapps::AppId& app_id,
    bool set_to_preferred,
    web_app::AllAppsLock& lock) {
  bool is_already_preferred = lock.registrar().CapturesLinksInScope(app_id);

  // Only update in web_app DB if the user selected choice does not match the
  // one in the DB currently.
  bool requires_update = (set_to_preferred && !is_already_preferred) ||
                         (!set_to_preferred && is_already_preferred);

  if (!requires_update) {
    return;
  }

  // TODO(b/273830801): Automatically call observers when changes are committed
  //  to the web_app DB.
  for (const webapps::AppId& id : lock.registrar().GetAppIds()) {
    if (id == app_id) {
      {
        web_app::ScopedRegistryUpdate update = lock.sync_bridge().BeginUpdate();
        web_app::WebApp* app_to_update = update->UpdateApp(app_id);
        app_to_update->SetIsUserSelectedAppForSupportedLinks(set_to_preferred);
      }
      lock.registrar().NotifyWebAppUserLinkCapturingPreferencesChanged(
          app_id, set_to_preferred);
    } else {
      // For all other app_ids, if one is already set as the preferred, reset
      // all other apps in the registry if they were previously set to be a
      // preferred app to capture similar type of links according to scope
      // prefixes.
      if (set_to_preferred && lock.registrar().CapturesLinksInScope(id) &&
          lock.registrar().AppScopesMatchForUserLinkCapturing(app_id, id)) {
        {
          web_app::ScopedRegistryUpdate update =
              lock.sync_bridge().BeginUpdate();
          web_app::WebApp* app_to_update = update->UpdateApp(id);
          app_to_update->SetIsUserSelectedAppForSupportedLinks(
              /*is_user_selected_app_for_capturing_links=*/false);
        }
        lock.registrar().NotifyWebAppUserLinkCapturingPreferencesChanged(
            id, /*is_preferred=*/false);
      }
    }
  }
}
#endif  // !BUILDFLAG(IS_CHROMEOS)
