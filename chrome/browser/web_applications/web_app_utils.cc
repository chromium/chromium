// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_utils.h"

#include <algorithm>
#include <functional>
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
#include "base/containers/extend.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/map_util.h"
#include "base/containers/to_vector.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/crx_file/id_util.h"
#include "components/grit/components_resources.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/run_on_os_login_types.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/webapps/browser/web_app_error_page_constants.h"
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

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "chromeos/ash/components/file_manager/app_id.h"
#include "components/user_manager/user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace web_app {

namespace {

#if BUILDFLAG(IS_CHROMEOS)

// This mapping excludes SWAs not included in official builds (like SAMPLE).
// These app Id constants need to be kept in sync with java/com/
// google/chrome/cros/policyconverter/ChromePolicySettingsProcessor.java
constexpr auto kSystemWebAppsMapping =
    base::MakeFixedFlatMap<std::string_view, ash::SystemWebAppType>(
        {{"file_manager", ash::SystemWebAppType::FILE_MANAGER},
         {"settings", ash::SystemWebAppType::SETTINGS},
         {"camera", ash::SystemWebAppType::CAMERA},
         {"terminal", ash::SystemWebAppType::TERMINAL},
         {"media", ash::SystemWebAppType::MEDIA},
         {"help", ash::SystemWebAppType::HELP},
         {"print_management", ash::SystemWebAppType::PRINT_MANAGEMENT},
         {"scanning", ash::SystemWebAppType::SCANNING},
         {"diagnostics", ash::SystemWebAppType::DIAGNOSTICS},
         {"connectivity_diagnostics",
          ash::SystemWebAppType::CONNECTIVITY_DIAGNOSTICS},
         {"eche", ash::SystemWebAppType::ECHE},
         {"crosh", ash::SystemWebAppType::CROSH},
         {"personalization", ash::SystemWebAppType::PERSONALIZATION},
         {"shortcut_customization",
          ash::SystemWebAppType::SHORTCUT_CUSTOMIZATION},
         {"shimless_rma", ash::SystemWebAppType::SHIMLESS_RMA},
         {"demo_mode", ash::SystemWebAppType::DEMO_MODE},
         {"os_feedback", ash::SystemWebAppType::OS_FEEDBACK},
         {"os_sanitize", ash::SystemWebAppType::OS_SANITIZE},
         {"projector", ash::SystemWebAppType::PROJECTOR},
         {"firmware_update", ash::SystemWebAppType::FIRMWARE_UPDATE},
         {"os_flags", ash::SystemWebAppType::OS_FLAGS},
         {"vc_background", ash::SystemWebAppType::VC_BACKGROUND},
         {"print_preview_cros", ash::SystemWebAppType::PRINT_PREVIEW_CROS},
         {"boca", ash::SystemWebAppType::BOCA},
         {"app_mall", ash::SystemWebAppType::MALL},
         {"recorder", ash::SystemWebAppType::RECORDER},
         {"graduation", ash::SystemWebAppType::GRADUATION}});

constexpr ash::SystemWebAppType GetMaxSystemWebAppType() {
  return std::ranges::max_element(
             kSystemWebAppsMapping, std::ranges::less{},
             &decltype(kSystemWebAppsMapping)::value_type::second)
      ->second;
}

static_assert(GetMaxSystemWebAppType() == ash::SystemWebAppType::kMaxValue,
              "Not all SWA types are listed in |system_web_apps_mapping|.");

#endif  // BUILDFLAG(IS_CHROMEOS)

// Note that this mapping lists only selected Preinstalled Web Apps
// actively used in policies and is not meant to be exhaustive.
// These app Id constants need to be kept in sync with java/com/
// google/chrome/cros/policyconverter/ChromePolicySettingsProcessor.java
// LINT.IfChange
constexpr auto kPreinstalledWebAppsMapping =
    base::MakeFixedFlatMap<std::string_view, std::string_view>(
        {{"cursive", ash::kCursiveAppId}, {"canvas", ash::kCanvasAppId}});

std::optional<base::flat_map<std::string_view, std::string_view>>&
GetPreinstalledWebAppsMappingForTesting() {
  static base::NoDestructor<
      std::optional<base::flat_map<std::string_view, std::string_view>>>
      preinstalled_web_apps_mapping_for_testing;
  return *preinstalled_web_apps_mapping_for_testing;
}
// LINT.ThenChange(//depot/google3/java/com/google/chrome/cros/policyconverter/ChromePolicySettingsProcessor.java)

GURL EncodeIconAsUrl(const SkBitmap& bitmap) {
  std::optional<std::vector<uint8_t>> output =
      gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, /*discard_transparency=*/false);
  std::string encoded =
      base::Base64Encode(output.value_or(std::vector<uint8_t>()));
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

}  // namespace

std::optional<std::string_view> GetPolicyIdForPreinstalledWebApp(
    std::string_view app_id) {
  if (const auto& test_mapping = GetPreinstalledWebAppsMappingForTesting()) {
    for (const auto& [policy_id, mapped_app_id] : *test_mapping) {
      if (mapped_app_id == app_id) {
        return policy_id;
      }
    }
    return {};
  }

  for (const auto& [policy_id, mapped_app_id] : kPreinstalledWebAppsMapping) {
    if (mapped_app_id == app_id) {
      return policy_id;
    }
  }
  return {};
}

void SetPreinstalledWebAppsMappingForTesting(  // IN-TEST
    std::optional<base::flat_map<std::string_view, std::string_view>>
        preinstalled_web_apps_mapping_for_testing) {
  GetPreinstalledWebAppsMappingForTesting() =                // IN-TEST
      std::move(preinstalled_web_apps_mapping_for_testing);  // IN-TEST
}

constexpr base::FilePath::CharType kManifestResourcesDirectoryName[] =
    FILE_PATH_LITERAL("Manifest Resources");

constexpr base::FilePath::CharType kTempDirectoryName[] =
    FILE_PATH_LITERAL("Temp");

bool AreWebAppsEnabled(Profile* profile) {
  if (!profile || profile->IsSystemProfile()) {
    return false;
  }

#if BUILDFLAG(IS_CHROMEOS)
  // Web Apps should not be installed to the ChromeOS system profiles except the
  // lock screen app profile.
  if (!ash::ProfileHelper::IsUserProfile(profile) &&
      !ash::IsShimlessRmaAppBrowserContext(profile)) {
    return false;
  }
  auto* user_manager = user_manager::UserManager::Get();

  // Don't enable for Chrome App Kiosk sessions.
  if (user_manager && user_manager->IsLoggedInAsKioskChromeApp()) {
    return false;
  }

  // Guest session forces OTR to be turned on.
  if (profile->IsGuestSession()) {
    return profile->IsOffTheRecord();
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  return !profile->IsOffTheRecord();
}

bool AreWebAppsUserInstallable(Profile* profile) {
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

  if (AreWebAppsEnabled(profile)) {
    return profile;
  }

  // On ChromeOS, the system web app implementation requires that incognito
  // profiles can be used to look up the WebAppProvider of their original
  // profile.
  // TODO(https://crbug.com/384063076): Stop returning for profiles on ChromeOS
  // where `AreWebAppsEnabled` returns `false`.
#if BUILDFLAG(IS_CHROMEOS)
  Profile* original_profile = profile->GetOriginalProfile();
  CHECK(original_profile);
  if (AreWebAppsEnabled(original_profile)) {
    return original_profile;
  }
#endif

  return nullptr;
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
#if BUILDFLAG(IS_CHROMEOS)
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
#if BUILDFLAG(IS_CHROMEOS)
  // On Chrome OS, sync always locally installs an app.
  return true;
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
  std::ranges::transform(
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

#if BUILDFLAG(IS_CHROMEOS)
std::optional<std::string_view> GetPolicyIdForSystemWebAppType(
    ash::SystemWebAppType swa_type) {
  for (const auto& [policy_id, mapped_swa_type] : kSystemWebAppsMapping) {
    if (mapped_swa_type == swa_type) {
      return policy_id;
    }
  }
  return {};
}
#endif  // BUILDFLAG(IS_CHROMEOS)

bool IsChromeAppPolicyId(std::string_view policy_id) {
  return crx_file::id_util::IdIsValid(policy_id);
}

#if BUILDFLAG(IS_CHROMEOS)
bool IsArcAppPolicyId(std::string_view policy_id) {
  return base::Contains(policy_id, '.') && !IsWebAppPolicyId(policy_id);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

bool IsWebAppPolicyId(std::string_view policy_id) {
  return GURL{policy_id}.is_valid();
}

#if BUILDFLAG(IS_CHROMEOS)
bool IsSystemWebAppPolicyId(std::string_view policy_id) {
  return base::Contains(kSystemWebAppsMapping, policy_id);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

bool IsPreinstalledWebAppPolicyId(std::string_view policy_id) {
  if (auto& mapping = GetPreinstalledWebAppsMappingForTesting()) {  // IN-TEST
    return base::Contains(*mapping, policy_id);
  }
  return base::Contains(kPreinstalledWebAppsMapping, policy_id);
}

bool IsIsolatedWebAppPolicyId(std::string_view policy_id) {
  return web_package::SignedWebBundleId::Create(policy_id).has_value();
}

std::vector<std::string> GetPolicyIds(Profile* profile, const WebApp& web_app) {
  const auto& app_id = web_app.app_id();
  WebAppRegistrar& web_app_registrar =
      WebAppProvider::GetForWebApps(profile)->registrar_unsafe();

  if (web_app_registrar.IsIsolated(app_id) &&
      web_app_registrar.IsInstalledByPolicy(app_id)) {
    // This is an IWA - and thus, web_bundle_id == policy_id == URL hostname
    return {web_app.start_url().host()};
  }

  std::vector<std::string> policy_ids;

  if (std::optional<std::string_view> preinstalled_web_app_policy_id =
          GetPolicyIdForPreinstalledWebApp(app_id)) {
    policy_ids.emplace_back(*preinstalled_web_app_policy_id);
  }

#if BUILDFLAG(IS_CHROMEOS)
  const auto& swa_data = web_app.client_data().system_web_app_data;
  if (swa_data) {
    const ash::SystemWebAppType swa_type = swa_data->system_app_type;
    const std::optional<std::string_view> swa_policy_id =
        GetPolicyIdForSystemWebAppType(swa_type);
    if (swa_policy_id) {
      policy_ids.emplace_back(*swa_policy_id);
    }

    // File Manager SWA uses File Manager Extension's ID for policy.
    if (swa_type == ash::SystemWebAppType::FILE_MANAGER) {
      policy_ids.push_back(file_manager::kFileManagerAppId);
    }
  }
#endif  // BUIDLFLAG(IS_CHROMEOS)

  for (const auto& [source, external_config] :
       web_app.management_to_external_config_map()) {
    if (!external_config.additional_policy_ids.empty()) {
      base::Extend(policy_ids, external_config.additional_policy_ids);
    }
  }

  if (!web_app_registrar.HasExternalAppWithInstallSource(
          app_id, ExternalInstallSource::kExternalPolicy)) {
    return policy_ids;
  }

  base::flat_map<webapps::AppId, base::flat_set<GURL>> installed_apps =
      web_app_registrar.GetExternallyInstalledApps(
          ExternalInstallSource::kExternalPolicy);
  if (auto* install_urls = base::FindOrNull(installed_apps, app_id)) {
    DCHECK(!install_urls->empty());
    base::Extend(policy_ids, base::ToVector(*install_urls, &GURL::spec));
  }

  return policy_ids;
}

bool IsInScope(const GURL& url, const GURL& scope) {
  if (!scope.is_valid()) {
    return false;
  }

  return base::StartsWith(url.spec(), scope.spec(),
                          base::CompareCase::SENSITIVE);
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
      web_app_registrar.FindBestAppWithUrlInScope(
          url, WebAppFilter::InstalledInChrome());
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

void ResetAllContentSettingsForWebApp(Profile* profile, const GURL& app_scope) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  for (int i = static_cast<int>(ContentSettingsType::kMinValue);
       i <= static_cast<int>(ContentSettingsType::kMaxValue); ++i) {
    ContentSettingsType content_type = static_cast<ContentSettingsType>(i);

    if (content_type == ContentSettingsType::MIXEDSCRIPT ||
        content_type == ContentSettingsType::PROTOCOL_HANDLERS) {
      // These types are excluded because one can't call
      // GetDefaultContentSetting() for them.
      continue;
    }

    // ContentSettingsType enum values may include deprecated types or other
    // that are not registered in the ContentSettingsRegistry.
    // `Get()` returns nullptr for unregistered types. Skip these, as they
    // cannot be managed or reset via HostContentSettingsMap.
    if (!content_settings::ContentSettingsRegistry::GetInstance()->Get(
            content_type)) {
      continue;
    }

    host_content_settings_map->SetContentSettingDefaultScope(
        app_scope, app_scope, content_type, CONTENT_SETTING_DEFAULT);
  }
}

// TODO(crbug.com/331208955): Remove after migration.
bool WillBeSystemWebApp(const webapps::AppId& app_id,
                        WebAppManagementTypes sources) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && BUILDFLAG(IS_CHROMEOS)
  return app_id == ash::kGeminiAppId && sources.Has(WebAppManagement::kDefault);
#else  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
       // && BUILDFLAG(IS_CHROMEOS)
  return false;
#endif
}

}  // namespace web_app
