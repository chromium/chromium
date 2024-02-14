// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_management/app_management_page_handler_base.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/message_formatter.h"
#include "base/logging.h"
#include "base/notreached.h"
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
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registrar_observer.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/permission.h"
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
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/web_app_service_ash.h"
#include "chromeos/crosapi/mojom/web_app_service.mojom.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/default_apps_util.h"
#endif

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

std::optional<std::string> MaybeFormatBytes(std::optional<uint64_t> bytes) {
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
      return std::nullopt;
    }
    return base::UTF16ToUTF8(ui::FormatBytes(signed_bytes));
  }

  return std::nullopt;
}

}  // namespace

AppManagementPageHandlerBase::~AppManagementPageHandlerBase() {}

void AppManagementPageHandlerBase::GetApps(GetAppsCallback callback) {
  std::vector<app_management::mojom::AppPtr> app_management_apps;

  apps::AppServiceProxyFactory::GetForProfile(profile_)
      ->AppRegistryCache()
      .ForEachApp([this, &app_management_apps](const apps::AppUpdate& update) {
        app_management::mojom::AppPtr app = CreateApp(update.AppId());

        if (app) {
          app_management_apps.push_back(std::move(app));
        }
      });

  std::move(callback).Run(std::move(app_management_apps));
}

void AppManagementPageHandlerBase::GetApp(const std::string& app_id,
                                          GetAppCallback callback) {
  std::move(callback).Run(CreateApp(app_id));
}

void AppManagementPageHandlerBase::GetSubAppToParentMap(
    GetSubAppToParentMapCallback callback) {
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile_);
  if (provider) {
    // Web apps are managed in the current process (Ash or Lacros).
    provider->scheduler().ScheduleCallbackWithResult(
        "AppManagementPageHandlerBase::GetSubAppToParentMap",
        web_app::AllAppsLockDescription(),
        base::BindOnce(
            [](web_app::AllAppsLock& lock, base::Value::Dict& debug_value) {
              return lock.registrar().GetSubAppToParentMap();
            }),
        /*on_complete=*/std::move(callback),
        /*arg_for_shutdown=*/base::flat_map<std::string, std::string>());
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

void AppManagementPageHandlerBase::GetExtensionAppPermissionMessages(
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

void AppManagementPageHandlerBase::SetPermission(
    const std::string& app_id,
    apps::PermissionPtr permission) {
  apps::AppServiceProxyFactory::GetForProfile(profile_)->SetPermission(
      app_id, std::move(permission));
}

void AppManagementPageHandlerBase::Uninstall(const std::string& app_id) {
  apps::AppServiceProxyFactory::GetForProfile(profile_)->Uninstall(
      app_id, apps::UninstallSource::kAppManagement,
      delegate_->GetUninstallAnchorWindow());
}

void AppManagementPageHandlerBase::OpenNativeSettings(
    const std::string& app_id) {
  apps::AppServiceProxyFactory::GetForProfile(profile_)->OpenNativeSettings(
      app_id);
}

void AppManagementPageHandlerBase::UpdateAppSize(const std::string& app_id) {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
  proxy->UpdateAppSize(app_id);
}

void AppManagementPageHandlerBase::SetFileHandlingEnabled(
    const std::string& app_id,
    bool enabled) {
  auto permission = std::make_unique<apps::Permission>(
      apps::PermissionType::kFileHandling, enabled,
      /*is_managed=*/false);
  apps::AppServiceProxyFactory::GetForProfile(profile_)->SetPermission(
      app_id, std::move(permission));
}

AppManagementPageHandlerBase::AppManagementPageHandlerBase(
    mojo::PendingReceiver<app_management::mojom::PageHandler> receiver,
    mojo::PendingRemote<app_management::mojom::Page> page,
    Profile* profile,
    Delegate& delegate)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      profile_(profile),
      delegate_(delegate) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile_);
  app_registry_cache_observer_.Observe(&proxy->AppRegistryCache());
}

app_management::mojom::AppPtr AppManagementPageHandlerBase::CreateApp(
    const std::string& app_id) {
  app_management::mojom::AppPtr app;

  apps::AppServiceProxyFactory::GetForProfile(profile_)
      ->AppRegistryCache()
      .ForOneApp(app_id, [this, &app](const apps::AppUpdate& update) {
        if (update.ShowInManagement().value_or(false) &&
            apps_util::IsInstalled(update.Readiness())) {
          app = CreateAppFromAppUpdate(update);
        }
      });

  return app;
}

void AppManagementPageHandlerBase::NotifyAppChanged(const std::string& app_id) {
  app_management::mojom::AppPtr app = CreateApp(app_id);

  if (!app) {
    return;
  }

  page_->OnAppChanged(std::move(app));
}

app_management::mojom::AppPtr
AppManagementPageHandlerBase::CreateAppFromAppUpdate(
    const apps::AppUpdate& update) {
  auto app = app_management::mojom::App::New();
  app->id = update.AppId();
  app->type = update.AppType();
  app->title = update.ShortName();

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

  app->hide_more_settings = ShouldHideMoreSettings(app->id);
  app->hide_pin_to_shelf =
      !update.ShowInShelf().value_or(true) || ShouldHidePinToShelf(app->id);
  app->window_mode = update.WindowMode();

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

      std::optional<GURL> learn_more_url;
      if (!CanShowDefaultAppAssociationsUi()) {
        learn_more_url = GURL(kFileHandlingLearnMore);
      }
      // TODO(crbug/1252505): add file handling policy support.
      app->file_handling_state = app_management::mojom::FileHandlingState::New(
          fh_enabled, /*is_managed=*/false, file_handling_types,
          file_handling_types_label, learn_more_url);
    }
  }

  app->publisher_id = update.PublisherId();

  return app;
}

void AppManagementPageHandlerBase::OnAppUpdate(const apps::AppUpdate& update) {
  app_management::mojom::AppPtr app = CreateApp(update.AppId());
  if (update.ShowInManagementChanged() || update.ReadinessChanged()) {
    if (update.ShowInManagement().value_or(false) &&
        update.Readiness() == apps::Readiness::kReady) {
      page_->OnAppAdded(std::move(app));
    }

    if (!update.ShowInManagement().value_or(true) ||
        !apps_util::IsInstalled(update.Readiness())) {
      page_->OnAppRemoved(update.AppId());
    }
  } else if (app) {
    page_->OnAppChanged(std::move(app));
  }
}

void AppManagementPageHandlerBase::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  cache->RemoveObserver(this);
}
