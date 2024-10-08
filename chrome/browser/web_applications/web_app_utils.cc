// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_utils.h"

#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <utility>

#include "ash/constants/web_app_id_constants.h"
#include "base/base64.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/enum_set.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/grit/generated_resources.h"
#include "components/grit/components_resources.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/run_on_os_login_types.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/alternative_error_page_override_info.mojom-forward.h"
#include "content/public/common/alternative_error_page_override_info.mojom.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-shared.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
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
// main profile. This may be modified by SetSkipMainProfileCheckForTesting().
bool g_skip_main_profile_check_for_testing = false;
#endif

GURL EncodeIconAsUrl(const SkBitmap& bitmap) {
  std::vector<unsigned char> output;
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &output);
  std::string encoded = base::Base64Encode(output);
  return GURL("data:image/png;base64," + encoded);
}

// This class is responsible for fetching the app icon for a web app and for
// providing it to the error page that's currently showing. The class
// monitors the lifetime of the web_contents for the page and deletes itself
// under these conditions:
//
// 1) It is unable to determine which icon to download.
// 2) The error page being monitored (it's web_contents) is destroyed.
// 3) The page starts loading something else.
// 4) (Success case) The icon is successfully fetched and delivered to the web
//    page.
//
// Note that this class can not rely on downloading the bits off the network
// because it has to work even when the app is launched for the first time while
// network is disconnected.
class AppIconFetcherTask : public content::WebContentsObserver {
 public:
  // Starts the asynchronous fetching of a specific web app icon from disk using
  // the `web_app_provider` and supplies the icon to the web_page via jscript.
  static void FetchAndPopulateIcon(content::WebContents* web_contents,
                                   WebAppProvider* web_app_provider,
                                   const webapps::AppId& app_id) {
    new AppIconFetcherTask(web_contents, web_app_provider, app_id);
  }

  AppIconFetcherTask() = delete;

 private:
  AppIconFetcherTask(content::WebContents* web_contents,
                     WebAppProvider* web_app_provider,
                     const webapps::AppId& app_id)
      : WebContentsObserver(web_contents) {
    DCHECK(web_contents);
    // For best results, this should be of equal (or slightly higher) value than
    // the width and height of the presented icon on the default offline error
    // page (see webapp_default_offline.[html|css] for icon details).
    const int kDesiredSizeForIcon = 160;
    web_app_provider->icon_manager().ReadIconAndResize(
        app_id, IconPurpose::ANY, kDesiredSizeForIcon,
        base::BindOnce(&AppIconFetcherTask::OnIconFetched,
                       weak_factory_.GetWeakPtr(), kDesiredSizeForIcon));
  }

  // WebContentsObserver:
  void WebContentsDestroyed() override { delete this; }

  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    // Loading will have started already when the error page is being
    // constructed, so if we receive this event, it means that a new navigation
    // is taking place (so we can drop any remaining work).
    if (navigation_handle->IsInPrimaryMainFrame()) {
      delete this;
    }
  }

  void DocumentOnLoadCompletedInPrimaryMainFrame() override {
    document_ready_ = true;
    MaybeSendImageAndSelfDestruct();
  }

  void OnIconFetched(int fetched_size,
                     std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
    DCHECK(icon_bitmaps.size() == 1);
    DCHECK(icon_bitmaps.begin()->first == fetched_size);
    if (icon_bitmaps.size() == 0) {
      delete this;
      return;
    }
    icon_url_ = EncodeIconAsUrl(icon_bitmaps.begin()->second);
    MaybeSendImageAndSelfDestruct();
  }

  // This function does nothing until both of these conditions have been met:
  // 1) The app icon image has been fetched.
  // 2) The error page is ready to receive the image.
  // Once they are met, this function will send the icon to the web page and
  // delete itself. Callers should not assume it is safe to do more work after
  // calling this function.
  void MaybeSendImageAndSelfDestruct() {
    if (!document_ready_ || icon_url_.is_empty()) {
      return;
    }
    DCHECK(web_contents());
    DCHECK(icon_url_.is_valid());

    std::u16string app_icon_inline =
        std::u16string(u"var icon = document.getElementById('icon');") +
        u"icon.src ='" + base::UTF8ToUTF16(icon_url_.spec()) + u"';";

    content::RenderFrameHost* host = web_contents()->GetPrimaryMainFrame();
    host->ExecuteJavaScriptInIsolatedWorld(app_icon_inline, base::DoNothing(),
                                           ISOLATED_WORLD_ID_EXTENSIONS);

    delete this;
  }

  // This url will contain the fetched icon bits inlined as a data: url.
  GURL icon_url_;

  // Whether the error page is ready to receive the icon.
  bool document_ready_ = false;

  // A weak factory for this class, must be last in the member list.
  base::WeakPtrFactory<AppIconFetcherTask> weak_factory_{this};
};

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
    case DisplayMode::kPictureInPicture:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case DisplayMode::kStandalone:
    case DisplayMode::kFullscreen:
      return DisplayMode::kStandalone;
    case DisplayMode::kWindowControlsOverlay:
      return DisplayMode::kWindowControlsOverlay;
    case DisplayMode::kTabbed:
      if (base::FeatureList::IsEnabled(blink::features::kDesktopPWAsTabStrip)) {
        return DisplayMode::kTabbed;
      } else {
        return DisplayMode::kStandalone;
      }
    case DisplayMode::kBorderless:
      return DisplayMode::kBorderless;
  }
}

std::optional<DisplayMode> TryResolveUserDisplayMode(
    mojom::UserDisplayMode user_display_mode) {
  switch (user_display_mode) {
    case mojom::UserDisplayMode::kBrowser:
      return DisplayMode::kBrowser;
    case mojom::UserDisplayMode::kTabbed:
      if (base::FeatureList::IsEnabled(
              features::kDesktopPWAsTabStripSettings)) {
        return DisplayMode::kTabbed;
      }
      // Treat as standalone.
      [[fallthrough]];
    case mojom::UserDisplayMode::kStandalone:
      break;
  }

  return std::nullopt;
}

std::optional<DisplayMode> TryResolveOverridesDisplayMode(
    const std::vector<DisplayMode>& display_mode_overrides) {
  for (DisplayMode override_display_mode : display_mode_overrides) {
    DisplayMode resolved_display_mode =
        ResolveAppDisplayModeForStandaloneLaunchContainer(
            override_display_mode);
    if (override_display_mode == resolved_display_mode) {
      return resolved_display_mode;
    }
  }

  return std::nullopt;
}

DisplayMode ResolveNonIsolatedEffectiveDisplayMode(
    DisplayMode app_display_mode,
    const std::vector<DisplayMode>& display_mode_overrides,
    mojom::UserDisplayMode user_display_mode) {
  const std::optional<DisplayMode> resolved_display_mode =
      TryResolveUserDisplayMode(user_display_mode);

  if (resolved_display_mode.has_value()) {
    return *resolved_display_mode;
  }

  const std::optional<DisplayMode> resolved_override_display_mode =
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

bool AreWebAppsEnabled(Profile* profile) {
  if (!profile || profile->IsSystemProfile()) {
    return false;
  }

  const Profile* original_profile = profile->GetOriginalProfile();
  DCHECK(!original_profile->IsOffTheRecord());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Web Apps should not be installed to the ChromeOS system profiles except the
  // lock screen app profile.
  if (!ash::ProfileHelper::IsUserProfile(original_profile) &&
      !ash::ProfileHelper::IsLockScreenAppProfile(profile) &&
      !ash::IsShimlessRmaAppBrowserContext(profile)) {
    return false;
  }
  auto* user_manager = user_manager::UserManager::Get();
  // Don't enable if SWAs in Kiosk session are disabled for the next session
  // types.
  if (!base::FeatureList::IsEnabled(ash::features::kKioskEnableSystemWebApps)) {
    // Don't enable for Chrome App Kiosk sessions.
    if (user_manager && user_manager->IsLoggedInAsKioskApp()) {
      return false;
    }
  }
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  // Disable web apps in the profile unless one of the following is true:
  // * the profile is the main one
  // * the testing condition is set
  if (!(profile->IsMainProfile() || g_skip_main_profile_check_for_testing)) {
    return false;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return true;
}

bool AreWebAppsUserInstallable(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // With Lacros, web apps are not installed using the Ash browser.
  if (IsWebAppsCrosapiEnabled()) {
    return false;
  }
  if (ash::ProfileHelper::IsLockScreenAppProfile(profile)) {
    return false;
  }
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
  if (!AreWebAppsEnabled(original_profile)) {
    return nullptr;
  }

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
  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile) {
    return nullptr;
  }
  if (!site_engagement::SiteEngagementService::IsEnabled()) {
    return nullptr;
  }
  if (profile->GetOriginalProfile()->IsGuestSession()) {
    return nullptr;
  }
  return GetBrowserContextForWebApps(context);
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
    const webapps::AppId& app_id) {
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
  if (new_handlers.empty()) {
    return true;
  }

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
        if (!base::Contains(extensions_set, new_extension)) {
          return false;
        }
      }
    }
  }

  return true;
}

std::tuple<std::u16string, size_t>
GetFileTypeAssociationsHandledByWebAppForDisplay(Profile* profile,
                                                 const webapps::AppId& app_id) {
  auto* provider = WebAppProvider::GetForLocalAppsUnchecked(profile);
  if (!provider) {
    return {};
  }

  const apps::FileHandlers* file_handlers =
      provider->registrar_unsafe().GetAppFileHandlers(app_id);

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
  base::ranges::transform(
      extensions, std::back_inserter(extensions_for_display),
      [](const std::string& extension) {
        return base::UTF8ToUTF16(base::ToUpperASCII(extension.substr(1)));
      });
  return extensions_for_display;
}

bool IsRunOnOsLoginModeEnabledForAutostart(RunOnOsLoginMode login_mode) {
  switch (login_mode) {
    case RunOnOsLoginMode::kWindowed:
      return true;
    case RunOnOsLoginMode::kMinimized:
      return true;
    case RunOnOsLoginMode::kNotRun:
      return false;
  }
}

#if BUILDFLAG(IS_CHROMEOS)
bool IsWebAppsCrosapiEnabled() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return crosapi::browser_util::IsLacrosEnabled();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* lacros_service = chromeos::LacrosService::Get();
  return lacros_service &&
         lacros_service->IsAvailable<crosapi::mojom::AppPublisher>();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void SetSkipMainProfileCheckForTesting(bool skip_check) {
  g_skip_main_profile_check_for_testing = skip_check;
}

bool IsMainProfileCheckSkippedForTesting() {
  return g_skip_main_profile_check_for_testing;
}
#endif

bool HasAnySpecifiedSourcesAndNoOtherSources(
    WebAppManagementTypes sources,
    WebAppManagementTypes specified_sources) {
  bool has_any_specified_sources = sources.HasAny(specified_sources);
  bool has_no_other_sources =
      base::Difference(sources, specified_sources).empty();
  return has_any_specified_sources && has_no_other_sources;
}

bool CanUserUninstallWebApp(const webapps::AppId& app_id,
                            WebAppManagementTypes sources) {
  return !WillBeSystemWebApp(app_id, sources) &&
         HasAnySpecifiedSourcesAndNoOtherSources(sources,
                                                 kUserUninstallableSources);
}

webapps::AppId GetAppIdFromAppSettingsUrl(const GURL& url) {
  // App Settings page is served under chrome://app-settings/<app-id>.
  // url.path() returns "/<app-id>" with a leading slash.
  std::string path = url.path();
  if (path.size() <= 1) {
    return webapps::AppId();
  }
  return path.substr(1);
}

bool IsInScope(const GURL& url, const GURL& scope) {
  if (!scope.is_valid()) {
    return false;
  }

  return base::StartsWith(url.spec(), scope.spec(),
                          base::CompareCase::SENSITIVE);
}

DisplayMode ResolveEffectiveDisplayMode(
    DisplayMode app_display_mode,
    const std::vector<DisplayMode>& app_display_mode_overrides,
    mojom::UserDisplayMode user_display_mode,
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
    case DisplayMode::kPictureInPicture:
      return apps::LaunchContainer::kLaunchContainerWindow;
    case DisplayMode::kUndefined:
      return apps::LaunchContainer::kLaunchContainerNone;
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

content::mojom::AlternativeErrorPageOverrideInfoPtr ConstructWebAppErrorPage(
    const GURL& url,
    content::RenderFrameHost* render_frame_host,
    content::BrowserContext* browser_context,
    std::u16string message,
    std::u16string supplementary_icon) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  WebAppProvider* web_app_provider = WebAppProvider::GetForWebApps(profile);
  if (web_app_provider == nullptr) {
    return nullptr;
  }

  WebAppRegistrar& web_app_registrar = web_app_provider->registrar_unsafe();
  const std::optional<webapps::AppId> app_id =
      web_app_registrar.FindAppWithUrlInScope(url);
  if (!app_id.has_value()) {
    return nullptr;
  }

  // Fetch the app icon asynchronously and provide it to the error page. The
  // web_contents check exists because not all unit tests set up a proper error
  // page.
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (web_contents) {
    AppIconFetcherTask::FetchAndPopulateIcon(web_contents, web_app_provider,
                                             app_id.value());
  }

  auto alternative_error_page_info =
      content::mojom::AlternativeErrorPageOverrideInfo::New();
  base::Value::Dict dict;
  dict.Set(error_page::kAppShortName,
           web_app_registrar.GetAppShortName(*app_id));
  dict.Set(error_page::kMessage, message);
  // Android uses kIconUrl to provide the icon url synchronously, because it
  // already available, but Desktop sends down a transparent 1x1 pixel instead
  // and then updates it asynchronously once it is available.
  dict.Set(error_page::kIconUrl,
           "data:image/"
           "png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAACklE"
           "QVR42mMAAQAABQABoIJXOQAAAABJRU5ErkJggg==");
  dict.Set(error_page::kSupplementaryIcon, supplementary_icon);
  alternative_error_page_info->alternative_error_page_params = std::move(dict);
  alternative_error_page_info->resource_id = IDR_WEBAPP_ERROR_PAGE_HTML;
  return alternative_error_page_info;
}

bool IsValidScopeForLinkCapturing(const GURL& scope) {
  return scope.is_valid() && scope.has_scheme() && scope.SchemeIsHTTPOrHTTPS();
}

// TODO(http://b/331208955): Remove after migration.
bool WillBeSystemWebApp(const webapps::AppId& app_id,
                        WebAppManagementTypes sources) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && BUILDFLAG(IS_CHROMEOS)
  return app_id == kContainerAppId && sources.Has(WebAppManagement::kDefault);
#else  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && BUILDFLAG(IS_CHROMEOS)
  return false;
#endif
}

}  // namespace web_app
