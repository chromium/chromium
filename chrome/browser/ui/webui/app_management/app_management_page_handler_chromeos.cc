// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_management/app_management_page_handler_chromeos.h"

#include <set>
#include <string>
#include <vector>

#include "ash/components/arc/app/arc_app_constants.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/apps/apk_web_app_service.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/web_app_service_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/app_management/app_management_page_handler_base.h"
#include "chrome/browser/ui/webui/app_management/app_management_shelf_delegate_chromeos.h"
#include "chrome/browser/ui/webui/ash/settings/os_settings_features_util.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chromeos/crosapi/mojom/web_app_service.mojom.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/preferred_apps_list_handle.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permission_message.h"
#include "extensions/common/permissions/permissions_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/webui/resources/cr_components/app_management/app_management.mojom.h"

namespace {

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

app_management::mojom::LocalePtr CreateLocaleForTag(
    const std::string& locale_tag,
    const std::string& system_locale) {
  const std::string display_name =
      base::UTF16ToUTF8(l10n_util::GetDisplayNameForLocale(
          locale_tag, system_locale, /*is_for_ui=*/true));
  const std::string native_display_name = base::UTF16ToUTF8(
      l10n_util::GetDisplayNameForLocale(locale_tag, locale_tag,
                                         /*is_for_ui=*/true));

  // In ICU library, undefined locale is treated as unknown language
  // (ICU-20273).
  constexpr char kUndefinedTranslatedLocaleName[] = "und";

  // In Android, it's possible for Apps to set custom locale tag, hence these
  // locales might be untranslatable (based on ICU-20273).
  // In this case, we'll pass empty string and let the UI decides what to
  // display. For ARC, we'll display the `locale_tag` as is (this is safe
  // within the limit specified by IETF BCP 47, as no malicious HTML tags
  // could be formed).
  return app_management::mojom::Locale::New(
      locale_tag,
      display_name == kUndefinedTranslatedLocaleName ? "" : display_name,
      native_display_name == kUndefinedTranslatedLocaleName
          ? ""
          : native_display_name);
}

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

std::optional<std::string> MaybeFormatBytes(std::optional<uint64_t> bytes) {
  if (bytes.has_value()) {
    // ui::FormatBytes requires a non-negative signed integer. In general, we
    // expect that converting from unsigned to signed int here should always
    // yield a positive value, since overflowing into negative would require an
    // implausibly large app (2^63 bytes ~= 9 exabytes).
    int64_t signed_bytes = static_cast<int64_t>(bytes.value());
    if (signed_bytes < 0) {
      // TODO(crbug.com/40063212): Investigate ARC apps which have negative data
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

AppManagementPageHandlerChromeOs::AppManagementPageHandlerChromeOs(
    mojo::PendingReceiver<app_management::mojom::PageHandler> receiver,
    mojo::PendingRemote<app_management::mojom::Page> page,
    Profile* profile,
    AppManagementPageHandlerBase::Delegate& delegate)
    : AppManagementPageHandlerBase(std::move(receiver),
                                   std::move(page),
                                   profile),
      shelf_delegate_(this, profile),
      delegate_(delegate) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  preferred_apps_list_handle_observer_.Observe(&proxy->PreferredAppsList());
}

AppManagementPageHandlerChromeOs::~AppManagementPageHandlerChromeOs() = default;

void AppManagementPageHandlerChromeOs::OnPinnedChanged(
    const std::string& app_id,
    bool pinned) {
  NotifyAppChanged(app_id);
}

void AppManagementPageHandlerChromeOs::GetSubAppToParentMap(
    GetSubAppToParentMapCallback callback) {
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile());
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

  // Reaching here means that WebAppProviderBridge and WebAppProvider were both
  // not found.
  std::move(callback).Run(base::flat_map<std::string, std::string>());
}

void AppManagementPageHandlerChromeOs::GetExtensionAppPermissionMessages(
    const std::string& app_id,
    GetExtensionAppPermissionMessagesCallback callback) {
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile());
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

void AppManagementPageHandlerChromeOs::SetPinned(const std::string& app_id,
                                                 bool pinned) {
  shelf_delegate_.SetPinned(app_id, pinned);
}

void AppManagementPageHandlerChromeOs::SetResizeLocked(
    const std::string& app_id,
    bool locked) {
  apps::AppServiceProxyFactory::GetForProfile(profile())->SetResizeLocked(
      app_id, locked);
}

void AppManagementPageHandlerChromeOs::Uninstall(const std::string& app_id) {
  auto* const proxy = apps::AppServiceProxyFactory::GetForProfile(profile());

  std::optional<bool> allow_uninstall;
  proxy->AppRegistryCache().ForOneApp(
      app_id, [&](const apps::AppUpdate& update) {
        allow_uninstall = update.AllowUninstall();
      });

  if (!allow_uninstall.value_or(false)) {
    mojo::ReportBadMessage("Invalid attempt to uninstall app.");
    return;
  }

  proxy->Uninstall(app_id, apps::UninstallSource::kAppManagement,
                   delegate_->GetUninstallAnchorWindow());
}

void AppManagementPageHandlerChromeOs::SetPreferredApp(
    const std::string& app_id,
    bool is_preferred_app) {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
  bool is_preferred_app_for_supported_links =
      proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_id);

  if (is_preferred_app && !is_preferred_app_for_supported_links) {
    proxy->SetSupportedLinksPreference(app_id);
  } else if (!is_preferred_app && is_preferred_app_for_supported_links) {
    proxy->RemoveSupportedLinksPreference(app_id);
  }
}

void AppManagementPageHandlerChromeOs::GetOverlappingPreferredApps(
    const std::string& app_id,
    GetOverlappingPreferredAppsCallback callback) {
  auto intent_filters = GetSupportedLinkIntentFilters(profile(), app_id);
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
  base::flat_set<std::string> app_ids =
      proxy->PreferredAppsList().FindPreferredAppsForFilters(intent_filters);
  app_ids.erase(app_id);

  // Erase all IDs that do not correspond to installed apps in App Service. Such
  // IDs could be apps that have been uninstalled but did not have their
  // preference updated correctly, or the legacy "use_browser" preference. This
  // prevents attempting to show an overlapping app dialog for an app that
  // doesn't currently exist.
  base::EraseIf(app_ids, [proxy](const std::string& app_id) {
    return !proxy->AppRegistryCache().IsAppInstalled(app_id);
  });
  std::move(callback).Run(std::move(app_ids).extract());
}

void AppManagementPageHandlerChromeOs::UpdateAppSize(
    const std::string& app_id) {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
  proxy->UpdateAppSize(app_id);
}

void AppManagementPageHandlerChromeOs::SetWindowMode(
    const std::string& app_id,
    apps::WindowMode window_mode) {
  NOTIMPLEMENTED();
}

void AppManagementPageHandlerChromeOs::SetRunOnOsLoginMode(
    const std::string& app_id,
    apps::RunOnOsLoginMode run_on_os_login_mode) {
  NOTIMPLEMENTED();
}

void AppManagementPageHandlerChromeOs::ShowDefaultAppAssociationsUi() {
  NOTIMPLEMENTED();
}

void AppManagementPageHandlerChromeOs::OpenStorePage(
    const std::string& app_id) {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
  auto* apk_service = ash::ApkWebAppService::Get(profile());
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
}

void AppManagementPageHandlerChromeOs::SetAppLocale(
    const std::string& app_id,
    const std::string& locale_tag) {
  apps::AppServiceProxyFactory::GetForProfile(profile())->SetAppLocale(
      app_id, locale_tag);
}

void AppManagementPageHandlerChromeOs::OnPreferredAppChanged(
    const std::string& app_id,
    bool is_preferred_app) {
  NotifyAppChanged(app_id);
}

void AppManagementPageHandlerChromeOs::OnPreferredAppsListWillBeDestroyed(
    apps::PreferredAppsListHandle* handle) {
  preferred_apps_list_handle_observer_.Reset();
}

app_management::mojom::AppPtr AppManagementPageHandlerChromeOs::CreateApp(
    const std::string& app_id) {
  app_management::mojom::AppPtr app =
      AppManagementPageHandlerBase::CreateApp(app_id);
  if (!app) {
    return nullptr;
  }

  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());

  app->is_preferred_app =
      proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_id);
  app->supported_links = GetSupportedLinks(profile(), app->id);

  app->is_pinned = shelf_delegate_.IsPinned(app_id);
  app->is_policy_pinned = shelf_delegate_.IsPolicyPinned(app_id);

  proxy->AppRegistryCache().ForOneApp(
      app_id, [this, &app](const apps::AppUpdate& update) {
        app->app_size = MaybeFormatBytes(update.AppSizeInBytes());
        app->data_size = MaybeFormatBytes(update.DataSizeInBytes());

        app->resize_locked = update.ResizeLocked().value_or(false);
        app->hide_resize_locked = !update.ResizeLocked().has_value();

        app->allow_uninstall = update.AllowUninstall().value_or(false);

        if (ash::settings::IsPerAppLanguageEnabled(profile())) {
          const std::string& system_locale =
              g_browser_process->GetApplicationLocale();
          // Translate supported locales.
          for (const std::string& locale_tag : update.SupportedLocales()) {
            app->supported_locales.push_back(
                CreateLocaleForTag(locale_tag, system_locale));
          }
          // Translate selected locale.
          std::optional<std::string> locale_tag = update.SelectedLocale();
          if (locale_tag.has_value()) {
            app->selected_locale =
                CreateLocaleForTag(*locale_tag, system_locale);
          }
        }
      });

  return app;
}
