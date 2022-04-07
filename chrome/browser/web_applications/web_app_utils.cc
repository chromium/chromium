// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_utils.h"

#include <utility>

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/grit/components_resources.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/strings/grit/components_strings.h"
#include "skia/ext/skia_utils_base.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/feature_list.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/common/chrome_features.h"
#include "components/user_manager/user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
bool g_enable_system_web_apps_in_lacros_for_testing = false;

// Denotes whether user web apps may be installed on profiles other than the
// main profile. This may be modified by SkipMainProfileCheckForTesting().
bool g_skip_main_profile_check_for_testing = false;
#endif

GURL EncodeIconAsUrl(const SkBitmap& bitmap) {
  std::vector<unsigned char> output;
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &output);
  std::string encoded;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(output.data()),
                        output.size()),
      &encoded);
  return GURL("data:image/png;base64," + encoded);
}

}  // namespace

namespace web_app {

constexpr base::FilePath::CharType kManifestResourcesDirectoryName[] =
    FILE_PATH_LITERAL("Manifest Resources");

constexpr base::FilePath::CharType kTempDirectoryName[] =
    FILE_PATH_LITERAL("Temp");

bool AreWebAppsEnabled(const Profile* profile) {
  if (!profile || profile->IsSystemProfile())
    return false;

  const Profile* original_profile = profile->GetOriginalProfile();
  DCHECK(!original_profile->IsOffTheRecord());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Web Apps should not be installed to the ChromeOS system profiles.
  if (!ash::ProfileHelper::IsRegularProfile(original_profile)) {
    return false;
  }
  // Disable Web Apps if running any kiosk app.
  auto* user_manager = user_manager::UserManager::Get();
  if (user_manager && user_manager->IsLoggedInAsAnyKioskApp()) {
    return false;
  }
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!profile->IsMainProfile() && !g_skip_main_profile_check_for_testing)
    return false;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return true;
}

bool AreWebAppsUserInstallable(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // With Lacros, web apps are not installed using the Ash browser.
  if (IsWebAppsCrosapiEnabled())
    return false;
#endif
  return AreWebAppsEnabled(profile) && !profile->IsGuestSession() &&
         !profile->IsOffTheRecord();
}

bool AreSystemWebAppsSupported() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!g_enable_system_web_apps_in_lacros_for_testing)
    return false;
#endif
  return true;
}

content::BrowserContext* GetBrowserContextForWebApps(
    content::BrowserContext* context) {
  // Use original profile to create only one KeyedService instance.
  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile) {
    return nullptr;
  }
  Profile* original_profile = profile->GetOriginalProfile();
  return AreWebAppsEnabled(original_profile) ? original_profile : nullptr;
}

content::BrowserContext* GetBrowserContextForWebAppMetrics(
    content::BrowserContext* context) {
  // Use original profile to create only one KeyedService instance.
  Profile* original_profile =
      Profile::FromBrowserContext(context)->GetOriginalProfile();
  const bool is_web_app_metrics_enabled =
      site_engagement::SiteEngagementService::IsEnabled() &&
      AreWebAppsEnabled(original_profile) &&
      !original_profile->IsGuestSession();
  return is_web_app_metrics_enabled ? original_profile : nullptr;
}

content::mojom::AlternativeErrorPageOverrideInfoPtr GetAppManifestInfo(
    const GURL& url,
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  web_app::WebAppProvider* web_app_provider =
      web_app::WebAppProvider::GetForWebApps(profile);
  if (web_app_provider == nullptr) {
    return nullptr;
  }

  web_app::WebAppRegistrar& web_app_registrar = web_app_provider->registrar();
  const absl::optional<web_app::AppId> app_id =
      web_app_registrar.FindAppWithUrlInScope(url);
  if (!app_id.has_value()) {
    return nullptr;
  }

  auto alternative_error_page_info =
      content::mojom::AlternativeErrorPageOverrideInfo::New();
  // TODO(crbug.com/1285128): Ensure sufficient contrast.
  base::Value dict(base::Value::Type::DICTIONARY);
  std::string theme_color = skia::SkColorToHexString(
      web_app_registrar.GetAppThemeColor(*app_id).value_or(SK_ColorBLACK));
  std::string background_color = skia::SkColorToHexString(
      web_app_registrar.GetAppBackgroundColor(*app_id).value_or(SK_ColorWHITE));
  dict.SetStringKey(default_offline::kThemeColor, theme_color);
  dict.SetStringKey(default_offline::kBackgroundColor, background_color);
  dict.SetStringKey(default_offline::kAppShortName,
                    web_app_registrar.GetAppShortName(*app_id));
  dict.SetStringKey(
      default_offline::kMessage,
      l10n_util::GetStringUTF16(IDS_ERRORPAGES_HEADING_INTERNET_DISCONNECTED));
  SkBitmap bitmap = web_app_provider->icon_manager().GetFavicon(*app_id);
  std::string icon_url = EncodeIconAsUrl(bitmap).spec();
  dict.SetStringKey(default_offline::kIconUrl, icon_url);
  absl::optional<SkColor> dark_mode_theme_color =
      web_app_registrar.GetAppDarkModeThemeColor(*app_id);
  if (dark_mode_theme_color) {
    dict.SetStringKey(default_offline::kDarkModeThemeColor,
                      skia::SkColorToHexString(dark_mode_theme_color.value()));
  } else {
    dict.SetStringKey(default_offline::kDarkModeThemeColor, theme_color);
  }
  absl::optional<SkColor> dark_mode_background_color =
      web_app_registrar.GetAppDarkModeThemeColor(*app_id);
  if (dark_mode_background_color) {
    dict.SetStringKey(
        default_offline::kDarkModeBackgroundColor,
        skia::SkColorToHexString(dark_mode_background_color.value()));
  } else {
    dict.SetStringKey(default_offline::kDarkModeBackgroundColor,
                      background_color);
  }
  alternative_error_page_info->alternative_error_page_params = std::move(dict);
  alternative_error_page_info->resource_id = IDR_WEBAPP_DEFAULT_OFFLINE_HTML;
  return alternative_error_page_info;
}

base::FilePath GetWebAppsRootDirectory(Profile* profile) {
  return profile->GetPath().Append(chrome::kWebAppDirname);
}

base::FilePath GetManifestResourcesDirectory(
    const base::FilePath& web_apps_root_directory) {
  return web_apps_root_directory.Append(kManifestResourcesDirectoryName);
}

base::FilePath GetManifestResourcesDirectory(Profile* profile) {
  return GetManifestResourcesDirectory(GetWebAppsRootDirectory(profile));
}

base::FilePath GetManifestResourcesDirectoryForApp(
    const base::FilePath& web_apps_root_directory,
    const AppId& app_id) {
  return GetManifestResourcesDirectory(web_apps_root_directory)
      .AppendASCII(app_id);
}

base::FilePath GetWebAppsTempDirectory(
    const base::FilePath& web_apps_root_directory) {
  return web_apps_root_directory.Append(kTempDirectoryName);
}

std::string GetProfileCategoryForLogging(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!ash::ProfileHelper::IsRegularProfile(profile)) {
    return "SigninOrLockScreen";
  } else if (user_manager::UserManager::Get()->IsLoggedInAsAnyKioskApp()) {
    return "Kiosk";
  } else if (ash::ProfileHelper::IsEphemeralUserProfile(profile)) {
    return "Ephemeral";
  } else if (ash::ProfileHelper::IsPrimaryProfile(profile)) {
    return "Primary";
  } else {
    return "Other";
  }
#else
  // Chrome OS profiles are different from non-ChromeOS ones. Because System Web
  // Apps are not installed on non Chrome OS, "Other" is returned here.
  return "Other";
#endif
}

bool IsChromeOsDataMandatory() {
#if BUILDFLAG(IS_CHROMEOS)
  return true;
#else
  return false;
#endif
}

bool AreAppsLocallyInstalledBySync() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // On Chrome OS, sync always locally installs an app.
  return true;
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  // With Crosapi, Ash no longer participates in sync.
  // On Chrome OS before Crosapi, sync always locally installs an app.
  return !IsWebAppsCrosapiEnabled();
#else
  return false;
#endif
}

bool AreNewFileHandlersASubsetOfOld(const apps::FileHandlers& old_handlers,
                                    const apps::FileHandlers& new_handlers) {
  if (new_handlers.empty())
    return true;

  const std::set<std::string> mime_types_set =
      apps::GetMimeTypesFromFileHandlers(old_handlers);
  const std::set<std::string> extensions_set =
      apps::GetFileExtensionsFromFileHandlers(old_handlers);

  for (const apps::FileHandler& new_handler : new_handlers) {
    for (const auto& new_handler_accept : new_handler.accept) {
      if (!base::Contains(mime_types_set, new_handler_accept.mime_type)) {
        return false;
      }

      for (const auto& new_extension : new_handler_accept.file_extensions) {
        if (!base::Contains(extensions_set, new_extension))
          return false;
      }
    }
  }

  return true;
}

std::tuple<std::u16string, size_t>
GetFileTypeAssociationsHandledByWebAppForDisplay(Profile* profile,
                                                 const AppId& app_id) {
  auto extensions =
      GetFileTypeAssociationsHandledByWebAppForDisplayAsList(profile, app_id);
  return {base::UTF8ToUTF16(base::JoinString(
              extensions, l10n_util::GetStringUTF8(
                              IDS_WEB_APP_FILE_HANDLING_LIST_SEPARATOR))),
          extensions.size()};
}

std::vector<std::string> GetFileTypeAssociationsHandledByWebAppForDisplayAsList(
    Profile* profile,
    const AppId& app_id) {
  auto* provider = WebAppProvider::GetForLocalAppsUnchecked(profile);
  if (!provider)
    return {};

  const apps::FileHandlers* file_handlers =
      provider->registrar().GetAppFileHandlers(app_id);

  std::set<std::string> extensions_set =
      apps::GetFileExtensionsFromFileHandlers(*file_handlers);
  std::vector<std::string> extensions_for_display;
  extensions_for_display.reserve(extensions_set.size());

  // Convert file types from formats like ".txt" to "TXT".
  std::transform(extensions_set.begin(), extensions_set.end(),
                 std::back_inserter(extensions_for_display),
                 [](const std::string& extension) {
                   return base::ToUpperASCII(extension.substr(1));
                 });
  return extensions_for_display;
}

#if BUILDFLAG(IS_CHROMEOS)
bool IsWebAppsCrosapiEnabled() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return base::FeatureList::IsEnabled(features::kWebAppsCrosapi) ||
         crosapi::browser_util::IsLacrosPrimaryBrowser();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* lacros_service = chromeos::LacrosService::Get();
  return lacros_service && lacros_service->init_params()->web_apps_enabled &&
         lacros_service->IsAvailable<crosapi::mojom::AppPublisher>();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void EnableSystemWebAppsInLacrosForTesting() {
  g_enable_system_web_apps_in_lacros_for_testing = true;
}

void SkipMainProfileCheckForTesting() {
  g_skip_main_profile_check_for_testing = true;
}
#endif

void PersistProtocolHandlersUserChoice(
    Profile* profile,
    const AppId& app_id,
    const GURL& protocol_url,
    bool allowed,
    base::OnceClosure update_finished_callback) {
  WebAppProvider* const provider = WebAppProvider::GetForWebApps(profile);
  DCHECK(provider);

  OsIntegrationManager& os_integration_manager =
      provider->os_integration_manager();
  const std::vector<custom_handlers::ProtocolHandler>
      original_protocol_handlers =
          os_integration_manager.GetAppProtocolHandlers(app_id);

  if (allowed) {
    provider->sync_bridge().AddAllowedLaunchProtocol(app_id,
                                                     protocol_url.scheme());
  } else {
    provider->sync_bridge().AddDisallowedLaunchProtocol(app_id,
                                                        protocol_url.scheme());
  }

  // OS protocol registration does not need to be updated.
  if (original_protocol_handlers ==
      os_integration_manager.GetAppProtocolHandlers(app_id)) {
    std::move(update_finished_callback).Run();
    return;
  }

  // TODO(https://crbug.com/1251062): Can we avoid the delay of startup, if the
  // action as allowed?
  provider->os_integration_manager().UpdateProtocolHandlers(
      app_id, /*force_shortcut_updates_if_needed=*/true,
      std::move(update_finished_callback));
}

void PersistFileHandlersUserChoice(Profile* profile,
                                   const AppId& app_id,
                                   bool allowed,
                                   base::OnceClosure update_finished_callback) {
  WebAppProvider* const provider = WebAppProvider::GetForWebApps(profile);
  DCHECK(provider);
  provider->sync_bridge().SetAppFileHandlerApprovalState(
      app_id,
      allowed ? ApiApprovalState::kAllowed : ApiApprovalState::kDisallowed);

  UpdateFileHandlerOsIntegration(provider, app_id,
                                 std::move(update_finished_callback));
}

void UpdateFileHandlerOsIntegration(
    WebAppProvider* provider,
    const AppId& app_id,
    base::OnceClosure update_finished_callback) {
  bool enabled =
      provider->os_integration_manager().IsFileHandlingAPIAvailable(app_id) &&
      !provider->registrar().IsAppFileHandlerPermissionBlocked(app_id);

  if (enabled ==
      provider->registrar().ExpectThatFileHandlersAreRegisteredWithOs(app_id)) {
    std::move(update_finished_callback).Run();
    return;
  }

  FileHandlerUpdateAction action = enabled ? FileHandlerUpdateAction::kUpdate
                                           : FileHandlerUpdateAction::kRemove;

#if BUILDFLAG(IS_MAC)
  // On Mac, the file handlers are encoded in the app shortcut. First
  // unregister the file handlers (verifying that it finishes synchronously),
  // then update the shortcut.
  Result unregister_file_handlers_result = Result::kError;
  provider->os_integration_manager().UpdateFileHandlers(
      app_id, action,
      base::BindOnce([](Result* result_out,
                        Result actual_result) { *result_out = actual_result; },
                     &unregister_file_handlers_result));
  DCHECK_EQ(Result::kOk, unregister_file_handlers_result);
  provider->os_integration_manager().UpdateShortcuts(
      app_id, /*old_name=*/{}, std::move(update_finished_callback));
#else
  provider->os_integration_manager().UpdateFileHandlers(
      app_id, action,
      base::BindOnce([](base::OnceClosure closure,
                        Result ignored) { std::move(closure).Run(); },
                     std::move(update_finished_callback)));
#endif

  DCHECK_EQ(
      enabled,
      provider->registrar().ExpectThatFileHandlersAreRegisteredWithOs(app_id));
}

bool HasAnySpecifiedSourcesAndNoOtherSources(WebAppSources sources,
                                             WebAppSources specified_sources) {
  bool has_any_specified_sources = (sources & specified_sources).any();
  bool has_no_other_sources = (sources & ~specified_sources).none();
  return has_any_specified_sources && has_no_other_sources;
}

bool CanUserUninstallWebApp(WebAppSources sources) {
  WebAppSources specified_sources;
  specified_sources[WebAppManagement::kDefault] = true;
  specified_sources[WebAppManagement::kSync] = true;
  specified_sources[WebAppManagement::kWebAppStore] = true;
  specified_sources[WebAppManagement::kSubApp] = true;
  return HasAnySpecifiedSourcesAndNoOtherSources(sources, specified_sources);
}

AppId GetAppIdFromAppSettingsUrl(const GURL& url) {
  // App Settings page is served under chrome://app-settings/<app-id>.
  // url.path() returns "/<app-id>" with a leading slash.
  std::string path = url.path();
  if (path.size() <= 1)
    return AppId();
  return path.substr(1);
}

bool HasAppSettingsPage(Profile* profile, const GURL& url) {
  const AppId app_id = GetAppIdFromAppSettingsUrl(url);
  if (app_id.empty())
    return false;

  WebAppProvider* provider = WebAppProvider::GetForWebApps(profile);
  if (!provider)
    return false;
  return provider->registrar().IsLocallyInstalled(app_id);
}

}  // namespace web_app
