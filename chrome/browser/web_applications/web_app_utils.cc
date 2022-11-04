// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_utils.h"

#include <algorithm>
#include <bitset>
#include <iterator>
#include <set>
#include <type_traits>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/containers/flat_tree.h"
#include "base/files/file_path.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sources.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/custom_handlers/protocol_handler.h"
#include "components/grit/components_resources.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/alternative_error_page_override_info.mojom-forward.h"
#include "content/public/common/alternative_error_page_override_info.mojom.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "skia/ext/skia_utils_base.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/feature_list.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "components/user_manager/user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_params_proxy.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace web_app {

namespace {

#if BUILDFLAG(IS_CHROMEOS_LACROS)
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

// Note: This can never return kBrowser. This is because the user has
// specified that the web app should be displayed in a window, and thus
// the lowest fallback that we can go to is kMinimalUi.
DisplayMode ResolveAppDisplayModeForStandaloneLaunchContainer(
    DisplayMode app_display_mode) {
  switch (app_display_mode) {
    case DisplayMode::kBrowser:
    case DisplayMode::kMinimalUi:
      return DisplayMode::kMinimalUi;
    case DisplayMode::kUndefined:
      NOTREACHED();
      [[fallthrough]];
    case DisplayMode::kStandalone:
    case DisplayMode::kFullscreen:
      return DisplayMode::kStandalone;
    case DisplayMode::kWindowControlsOverlay:
      return DisplayMode::kWindowControlsOverlay;
    case DisplayMode::kTabbed:
      if (base::FeatureList::IsEnabled(features::kDesktopPWAsTabStrip))
        return DisplayMode::kTabbed;
      else
        return DisplayMode::kStandalone;
    case DisplayMode::kBorderless:
      return DisplayMode::kBorderless;
  }
}

absl::optional<DisplayMode> TryResolveUserDisplayMode(
    UserDisplayMode user_display_mode) {
  switch (user_display_mode) {
    case UserDisplayMode::kBrowser:
      return DisplayMode::kBrowser;
    case UserDisplayMode::kTabbed:
      if (base::FeatureList::IsEnabled(features::kDesktopPWAsTabStripSettings))
        return DisplayMode::kTabbed;
      // Treat as standalone.
      [[fallthrough]];
    case UserDisplayMode::kStandalone:
      break;
  }

  return absl::nullopt;
}

absl::optional<DisplayMode> TryResolveOverridesDisplayMode(
    const std::vector<DisplayMode>& display_mode_overrides) {
  for (DisplayMode override_display_mode : display_mode_overrides) {
    DisplayMode resolved_display_mode =
        ResolveAppDisplayModeForStandaloneLaunchContainer(
            override_display_mode);
    if (override_display_mode == resolved_display_mode) {
      return resolved_display_mode;
    }
  }

  return absl::nullopt;
}

DisplayMode ResolveNonIsolatedEffectiveDisplayMode(
    DisplayMode app_display_mode,
    const std::vector<DisplayMode>& display_mode_overrides,
    UserDisplayMode user_display_mode) {
  const absl::optional<DisplayMode> resolved_display_mode =
      TryResolveUserDisplayMode(user_display_mode);
  if (resolved_display_mode.has_value()) {
    return *resolved_display_mode;
  }

  const absl::optional<DisplayMode> resolved_override_display_mode =
      TryResolveOverridesDisplayMode(display_mode_overrides);
  if (resolved_override_display_mode.has_value()) {
    return *resolved_override_display_mode;
  }

  return ResolveAppDisplayModeForStandaloneLaunchContainer(app_display_mode);
}

}  // namespace

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
  // Web Apps should not be installed to the ChromeOS system profiles except the
  // lock screen app profile.
  if (!ash::ProfileHelper::IsUserProfile(original_profile) &&
      !ash::ProfileHelper::IsLockScreenAppProfile(profile)) {
    return false;
  }
  auto* user_manager = user_manager::UserManager::Get();
  // Don't enable for Chrome App Kiosk sessions.
  if (user_manager && user_manager->IsLoggedInAsKioskApp())
    return false;
  // Don't enable for ARC Kiosk sessions.
  if (user_manager && user_manager->IsLoggedInAsArcKioskApp())
    return false;
  // Don't enable for Web Kiosk if kKioskEnableAppService is disabled.
  if (user_manager && user_manager->IsLoggedInAsWebKioskApp() &&
      !base::FeatureList::IsEnabled(features::kKioskEnableAppService))
    return false;
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
  if (ash::ProfileHelper::IsLockScreenAppProfile(profile))
    return false;
#endif
  return AreWebAppsEnabled(profile) && !profile->IsGuestSession() &&
         !profile->IsOffTheRecord();
}

content::BrowserContext* GetBrowserContextForWebApps(
    content::BrowserContext* context) {
  // Use original profile to create only one KeyedService instance.
  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile) {
    return nullptr;
  }
  Profile* original_profile = profile->GetOriginalProfile();
  if (!AreWebAppsEnabled(original_profile))
    return nullptr;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Use OTR profile for Guest Session.
  if (profile->IsGuestSession()) {
    return profile->IsOffTheRecord() ? profile : nullptr;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return original_profile;
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

content::mojom::AlternativeErrorPageOverrideInfoPtr GetOfflinePageInfo(
    const GURL& url,
    content::RenderFrameHost* render_frame_host,
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  WebAppProvider* web_app_provider = WebAppProvider::GetForWebApps(profile);
  if (web_app_provider == nullptr) {
    return nullptr;
  }

  WebAppRegistrar& web_app_registrar = web_app_provider->registrar();
  const absl::optional<AppId> app_id =
      web_app_registrar.FindAppWithUrlInScope(url);
  if (!app_id.has_value()) {
    return nullptr;
  }

  auto alternative_error_page_info =
      content::mojom::AlternativeErrorPageOverrideInfo::New();
  // TODO(crbug.com/1285128): Ensure sufficient contrast.
  base::Value::Dict dict;
  dict.Set(default_offline::kAppShortName,
           web_app_registrar.GetAppShortName(*app_id));
  dict.Set(default_offline::kMessage,
           l10n_util::GetStringUTF16(IDS_ERRORPAGES_HEADING_YOU_ARE_OFFLINE));
  // TODO(crbug.com/1285723): The FavIcon is not the right icon to use here, as
  // the design calls for showing an icon around ten times that size. This will
  // probably need to be changed to fetch the right icon asynchronously.
  SkBitmap bitmap = web_app_provider->icon_manager().GetFavicon(*app_id);
  std::string icon_url = EncodeIconAsUrl(bitmap).spec();
  dict.Set(default_offline::kIconUrl, icon_url);
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
  if (!ash::ProfileHelper::IsUserProfile(profile)) {
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
  auto* provider = WebAppProvider::GetForLocalAppsUnchecked(profile);
  if (!provider)
    return {};

  const apps::FileHandlers* file_handlers =
      provider->registrar().GetAppFileHandlers(app_id);

  std::vector<std::u16string> extensions_for_display =
      TransformFileExtensionsForDisplay(
          apps::GetFileExtensionsFromFileHandlers(*file_handlers));

  return {base::JoinString(extensions_for_display,
                           l10n_util::GetStringUTF16(
                               IDS_WEB_APP_FILE_HANDLING_LIST_SEPARATOR)),
          extensions_for_display.size()};
}

std::vector<std::u16string> TransformFileExtensionsForDisplay(
    const std::set<std::string>& extensions) {
  std::vector<std::u16string> extensions_for_display;
  extensions_for_display.reserve(extensions.size());
  std::transform(
      extensions.begin(), extensions.end(),
      std::back_inserter(extensions_for_display),
      [](const std::string& extension) {
        return base::UTF8ToUTF16(base::ToUpperASCII(extension.substr(1)));
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
  return chromeos::BrowserParamsProxy::Get()->WebAppsEnabled() &&
         lacros_service &&
         lacros_service->IsAvailable<crosapi::mojom::AppPublisher>();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
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

bool HasAnySpecifiedSourcesAndNoOtherSources(WebAppSources sources,
                                             WebAppSources specified_sources) {
  bool has_any_specified_sources = (sources & specified_sources).any();
  bool has_no_other_sources = (sources & ~specified_sources).none();
  return has_any_specified_sources && has_no_other_sources;
}

bool CanUserUninstallWebApp(WebAppSources sources) {
  WebAppSources specified_sources;
  for (WebAppManagement::Type type : {
           WebAppManagement::kDefault,
           WebAppManagement::kSync,
           WebAppManagement::kWebAppStore,
           WebAppManagement::kSubApp,
           WebAppManagement::kOem,
           WebAppManagement::kCommandLine,
       }) {
    specified_sources.set(type);
  }

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

bool IsInScope(const GURL& url, const GURL& scope) {
  if (!scope.is_valid())
    return false;

  return base::StartsWith(url.spec(), scope.spec(),
                          base::CompareCase::SENSITIVE);
}

DisplayMode ResolveEffectiveDisplayMode(
    DisplayMode app_display_mode,
    const std::vector<DisplayMode>& app_display_mode_overrides,
    UserDisplayMode user_display_mode,
    bool is_isolated) {
  const DisplayMode resolved_display_mode =
      ResolveNonIsolatedEffectiveDisplayMode(
          app_display_mode, app_display_mode_overrides, user_display_mode);
  if (is_isolated && resolved_display_mode == DisplayMode::kBrowser) {
    return DisplayMode::kStandalone;
  }

  return resolved_display_mode;
}

apps::LaunchContainer ConvertDisplayModeToAppLaunchContainer(
    DisplayMode display_mode) {
  switch (display_mode) {
    case DisplayMode::kBrowser:
      return apps::LaunchContainer::kLaunchContainerTab;
    case DisplayMode::kMinimalUi:
    case DisplayMode::kStandalone:
    case DisplayMode::kFullscreen:
    case DisplayMode::kWindowControlsOverlay:
    case DisplayMode::kTabbed:
    case DisplayMode::kBorderless:
      return apps::LaunchContainer::kLaunchContainerWindow;
    case DisplayMode::kUndefined:
      return apps::LaunchContainer::kLaunchContainerNone;
  }
}

std::string RunOnOsLoginModeToString(RunOnOsLoginMode mode) {
  switch (mode) {
    case RunOnOsLoginMode::kWindowed:
      return "windowed";
    case RunOnOsLoginMode::kMinimized:
      return "minimized";
    case RunOnOsLoginMode::kNotRun:
      return "not run";
  }
}

apps::RunOnOsLoginMode ConvertOsLoginMode(RunOnOsLoginMode login_mode) {
  switch (login_mode) {
    case RunOnOsLoginMode::kWindowed:
      return apps::RunOnOsLoginMode::kWindowed;
    case RunOnOsLoginMode::kNotRun:
      return apps::RunOnOsLoginMode::kNotRun;
    case RunOnOsLoginMode::kMinimized:
      return apps::RunOnOsLoginMode::kUnknown;
  }
}

const char* IconsDownloadedResultToString(IconsDownloadedResult result) {
  switch (result) {
    case IconsDownloadedResult::kCompleted:
      return "Completed";
    case IconsDownloadedResult::kPrimaryPageChanged:
      return "PrimaryPageChanged";
    case IconsDownloadedResult::kAbortedDueToFailure:
      return "AbortedDueToFailure";
  }
}

}  // namespace web_app
