// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/devtools/devtools_ui_bindings.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_internals_ui.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_safe_browsing_local_state_delegate.h"
#include "chrome/browser/ui/webui/about/about_ui.h"
#include "chrome/browser/ui/webui/components/components_ui.h"
#include "chrome/browser/ui/webui/crashes_ui.h"
#include "chrome/browser/ui/webui/download_internals/download_internals_ui.h"
#include "chrome/browser/ui/webui/flags/flags_ui.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/commerce/content/browser/commerce_internals_ui.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/product_specifications/product_specifications_set.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_util.h"
#include "components/favicon_base/select_favicon_frames.h"
#include "components/grit/components_scaled_resources.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/history_clusters_internals/webui/history_clusters_internals_ui.h"
#include "components/history_clusters/history_clusters_internals/webui/url_constants.h"
#include "components/lens/buildflags.h"
#include "components/optimization_guide/optimization_guide_internals/webui/url_constants.h"
#include "components/password_manager/content/common/web_ui_constants.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/reading_list/features/reading_list_switches.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/common/web_ui_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_utils.h"
#include "crypto/crypto_buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "media/media_buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/favicon_size.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/feed/feed_feature_list.h"
#else  // BUILDFLAG(IS_ANDROID)
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/app_service_internals/app_service_internals_ui.h"
#include "chrome/browser/ui/webui/bookmarks/bookmarks_ui.h"
#include "chrome/browser/ui/webui/devtools/devtools_ui.h"
#include "chrome/browser/ui/webui/downloads/downloads_ui.h"
#include "chrome/browser/ui/webui/history/history_ui.h"
#include "chrome/browser/ui/webui/identity_internals_ui.h"
#include "chrome/browser/ui/webui/inspect_ui.h"
#include "chrome/browser/ui/webui/management/management_ui.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/ui/webui/ntp/ntp_resource_cache.h"
#include "chrome/browser/ui/webui/password_manager/password_manager_ui.h"
#include "chrome/browser/ui/webui/settings/settings_utils.h"
#include "chrome/browser/ui/webui/signin/sync_confirmation_ui.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "media/base/media_switches.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/url_constants.h"
#include "ash/webui/camera_app_ui/url_constants.h"
#include "ash/webui/file_manager/url_constants.h"
#include "ash/webui/files_internals/url_constants.h"
#include "ash/webui/growth_internals/constants.h"
#include "ash/webui/help_app_ui/url_constants.h"
#include "ash/webui/mall/url_constants.h"
#include "ash/webui/multidevice_debug/url_constants.h"
#include "ash/webui/print_preview_cros/url_constants.h"
#include "ash/webui/recorder_app_ui/url_constants.h"
#include "ash/webui/vc_background_ui/url_constants.h"
#include "chrome/browser/ash/extensions/url_constants.h"
#include "chrome/browser/extensions/extension_keeplist_chromeos.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/crosapi/cpp/gurl_os_handler_utils.h"
#include "url/url_util.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/app_home/app_home_ui.h"
#include "chrome/browser/ui/webui/welcome/helpers.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/ui/webui/conflicts/conflicts_ui.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/webui/commerce/product_specifications_ui.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/sandbox/sandbox_internals_ui.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/ui/webui/whats_new/whats_new_ui.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/ui/webui/extensions/extensions_ui.h"
#include "extensions/browser/extension_registry.h"  // nogncheck
#include "extensions/browser/extension_system.h"    // nogncheck
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/ui/webui/welcome/helpers.h"
#include "chrome/browser/ui/webui/welcome/welcome_ui.h"
#endif

using content::WebUI;
using content::WebUIController;
using ui::WebDialogUI;

namespace {

// TODO(crbug.com/40214184): Allow a way to disable CSP in tests.
void SetUpWebUIDataSource(WebUI* web_ui,
                          const char* web_ui_host,
                          base::span<const webui::ResourcePath> resources,
                          int default_resource) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), web_ui_host);
  webui::SetupWebUIDataSource(source, resources, default_resource);
}

// A function for creating a new WebUI. The caller owns the return value, which
// may be nullptr (for example, if the URL refers to an non-existent extension).
typedef WebUIController* (*WebUIFactoryFunction)(WebUI* web_ui,
                                                 const GURL& url);

// Template for defining WebUIFactoryFunction.
template <class T>
WebUIController* NewWebUI(WebUI* web_ui, const GURL& url) {
  return new T(web_ui);
}

// Template for handlers defined in a component layer, that take an instance of
// a delegate implemented in the chrome layer.
template <class WEB_UI_CONTROLLER, class DELEGATE>
WebUIController* NewComponentUI(WebUI* web_ui, const GURL& url) {
  auto delegate = std::make_unique<DELEGATE>(web_ui);
  return new WEB_UI_CONTROLLER(web_ui, std::move(delegate));
}

template <>
WebUIController* NewWebUI<commerce::CommerceInternalsUI>(WebUI* web_ui,
                                                         const GURL& url) {
  Profile* profile = Profile::FromWebUI(web_ui);
  return new commerce::CommerceInternalsUI(
      web_ui,
      base::BindOnce(&SetUpWebUIDataSource, web_ui,
                     commerce::kChromeUICommerceInternalsHost),
      commerce::ShoppingServiceFactory::GetForBrowserContext(profile));
}

template <>
WebUIController* NewWebUI<OptimizationGuideInternalsUI>(WebUI* web_ui,
                                                        const GURL& url) {
  return OptimizationGuideInternalsUI::MaybeCreateOptimizationGuideInternalsUI(
      web_ui, base::BindOnce(&SetUpWebUIDataSource, web_ui,
                             optimization_guide_internals::
                                 kChromeUIOptimizationGuideInternalsHost));
}

template <>
WebUIController* NewWebUI<HistoryClustersInternalsUI>(WebUI* web_ui,
                                                      const GURL& url) {
  Profile* profile = Profile::FromWebUI(web_ui);
  return new HistoryClustersInternalsUI(
      web_ui, HistoryClustersServiceFactory::GetForBrowserContext(profile),
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS),
      base::BindOnce(
          &SetUpWebUIDataSource, web_ui,
          history_clusters_internals::kChromeUIHistoryClustersInternalsHost));
}

// Returns a function that can be used to create the right type of WebUI for a
// tab, based on its URL. Returns nullptr if the URL doesn't have WebUI
// associated with it.
WebUIFactoryFunction GetWebUIFactoryFunction(WebUI* web_ui,
                                             Profile* profile,
                                             const GURL& url) {
  // This will get called a lot to check all URLs, so do a quick check of other
  // schemes to filter out most URLs.
  if (!content::HasWebUIScheme(url))
    return nullptr;

  // This factory doesn't support chrome-untrusted:// WebUIs.
  if (url.SchemeIs(content::kChromeUIUntrustedScheme))
    return nullptr;

  // Please keep this in alphabetical order. If #ifs or special logics are
  // required, add it below in the appropriate section.
  //
  // We must compare hosts only since some of the Web UIs append extra stuff
  // after the host name.
  if (url.host_piece() == commerce::kChromeUICommerceInternalsHost) {
    return &NewWebUI<commerce::CommerceInternalsUI>;
  }
  if (url.host_piece() ==
      optimization_guide_internals::kChromeUIOptimizationGuideInternalsHost) {
    return &NewWebUI<OptimizationGuideInternalsUI>;
  }
  if (url.host_piece() == safe_browsing::kChromeUISafeBrowsingHost)
    return &NewComponentUI<safe_browsing::SafeBrowsingUI,
                           ChromeSafeBrowsingLocalStateDelegate>;
  if (url.host_piece() ==
      history_clusters_internals::kChromeUIHistoryClustersInternalsHost) {
    return &NewWebUI<HistoryClustersInternalsUI>;
  }

#if !BUILDFLAG(IS_ANDROID)
  if (url.host_piece() == chrome::kChromeUINewTabHost) {
    // The URL chrome://newtab/ can be either a virtual or a real URL,
    // depending on the context. In this case, it is always a real URL that
    // points to the New Tab page for the incognito profile only. For other
    // profile types, this URL must already be redirected to a different URL
    // that matches the profile type.
    //
    // Returning NewWebUI<NewTabUI> for the wrong profile type will lead to
    // crash in NTPResourceCache::GetNewTabHTML (Check: false), so here we add
    // a sanity check to prevent further crashes.
    //
    // The switch statement below must be consistent with the code in
    // NTPResourceCache::GetNewTabHTML!
    switch (NTPResourceCache::GetWindowType(profile)) {
      case NTPResourceCache::NORMAL:
        LOG(ERROR) << "Requested load of chrome://newtab/ for incorrect "
                      "profile type.";
        // TODO(crbug.com/40244589): Add DumpWithoutCrashing() here.
        return nullptr;
      case NTPResourceCache::INCOGNITO:
        [[fallthrough]];
      case NTPResourceCache::GUEST:
        [[fallthrough]];
      case NTPResourceCache::NON_PRIMARY_OTR:
        return &NewWebUI<NewTabUI>;
    }
  }
  if (url.SchemeIs(content::kChromeDevToolsScheme)) {
    if (!DevToolsUIBindings::IsValidFrontendURL(url))
      return nullptr;
    return &NewWebUI<DevToolsUI>;
  }
  // chrome://inspect isn't supported on Android nor iOS. Page debugging is
  // handled by a remote devtools on the host machine, and other elements, i.e.
  // extensions aren't supported.
  if (url.host_piece() == chrome::kChromeUIInspectHost)
    return &NewWebUI<InspectUI>;
  if (url.host_piece() == chrome::kChromeUISyncConfirmationHost &&
      !profile->IsOffTheRecord()) {
    return &NewWebUI<SyncConfirmationUI>;
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  return nullptr;
}

}  // namespace

WebUI::TypeID ChromeWebUIControllerFactory::GetWebUIType(
    content::BrowserContext* browser_context,
    const GURL& url) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  WebUIFactoryFunction function =
      GetWebUIFactoryFunction(nullptr, profile, url);
  return function ? reinterpret_cast<WebUI::TypeID>(function) : WebUI::kNoWebUI;
}

bool ChromeWebUIControllerFactory::UseWebUIForURL(
    content::BrowserContext* browser_context,
    const GURL& url) {
  return GetWebUIType(browser_context, url) != WebUI::kNoWebUI;
}

std::unique_ptr<WebUIController>
ChromeWebUIControllerFactory::CreateWebUIControllerForURL(WebUI* web_ui,
                                                          const GURL& url) {
  Profile* profile = Profile::FromWebUI(web_ui);
  WebUIFactoryFunction function = GetWebUIFactoryFunction(web_ui, profile, url);
  if (!function)
    return nullptr;

  return base::WrapUnique((*function)(web_ui, url));
}

void ChromeWebUIControllerFactory::GetFaviconForURL(
    Profile* profile,
    const GURL& page_url,
    const std::vector<int>& desired_sizes_in_pixel,
    favicon_base::FaviconResultsCallback callback) const {
  // Before determining whether page_url is an extension url, we must handle
  // overrides. This changes urls in |kChromeUIScheme| to extension urls, and
  // allows to use ExtensionWebUI::GetFaviconForURL.
  GURL url(page_url);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  ExtensionWebUI::HandleChromeURLOverride(&url, profile);

  // All extensions get their favicon from the icons part of the manifest.
  if (url.SchemeIs(extensions::kExtensionScheme)) {
    ExtensionWebUI::GetFaviconForURL(profile, url, std::move(callback));
    return;
  }
#endif

  std::vector<favicon_base::FaviconRawBitmapResult> favicon_bitmap_results;

  // Use ui::GetSupportedResourceScaleFactors instead of
  // favicon_base::GetFaviconScales() because chrome favicons comes from
  // resources.
  const std::vector<ui::ResourceScaleFactor>& resource_scale_factors =
      ui::GetSupportedResourceScaleFactors();

  std::vector<gfx::Size> candidate_sizes;
  for (const auto scale_factor : resource_scale_factors) {
    float scale = ui::GetScaleForResourceScaleFactor(scale_factor);
    // Assume that GetFaviconResourceBytes() returns favicons which are
    // |gfx::kFaviconSize| x |gfx::kFaviconSize| DIP.
    int candidate_edge_size =
        static_cast<int>(gfx::kFaviconSize * scale + 0.5f);
    candidate_sizes.push_back(
        gfx::Size(candidate_edge_size, candidate_edge_size));
  }
  std::vector<size_t> selected_indices;
  SelectFaviconFrameIndices(candidate_sizes, desired_sizes_in_pixel,
                            &selected_indices, nullptr);
  for (size_t selected_index : selected_indices) {
    ui::ResourceScaleFactor selected_resource_scale =
        resource_scale_factors[selected_index];

    scoped_refptr<base::RefCountedMemory> bitmap(
        GetFaviconResourceBytes(url, selected_resource_scale));
    if (bitmap.get() && bitmap->size()) {
      favicon_base::FaviconRawBitmapResult bitmap_result;
      bitmap_result.bitmap_data = bitmap;
      // Leave |bitmap_result|'s icon URL as the default of GURL().
      bitmap_result.icon_type = favicon_base::IconType::kFavicon;
      bitmap_result.pixel_size = candidate_sizes[selected_index];
      favicon_bitmap_results.push_back(bitmap_result);
    }
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(favicon_bitmap_results)));
}

// static
ChromeWebUIControllerFactory* ChromeWebUIControllerFactory::GetInstance() {
  static base::NoDestructor<ChromeWebUIControllerFactory> instance;
  return instance.get();
}

// static
bool ChromeWebUIControllerFactory::IsWebUIAllowedToMakeNetworkRequests(
    const url::Origin& origin) {
  // Allowlist to work around exceptional cases.
  //
  // If you are adding a new host to this list, please file a corresponding bug
  // to track its removal. See https://crbug.com/829412 for the metabug.
  return
      // https://crbug.com/831812
      origin.host() == chrome::kChromeUISyncConfirmationHost ||
      // https://crbug.com/831813
      origin.host() == chrome::kChromeUIInspectHost ||
      // https://crbug.com/859345
      origin.host() == chrome::kChromeUIDownloadsHost;
}

ChromeWebUIControllerFactory::ChromeWebUIControllerFactory() = default;

ChromeWebUIControllerFactory::~ChromeWebUIControllerFactory() = default;

base::RefCountedMemory* ChromeWebUIControllerFactory::GetFaviconResourceBytes(
    const GURL& page_url,
    ui::ResourceScaleFactor scale_factor) const {
#if !BUILDFLAG(IS_ANDROID)
  // The extension scheme is handled in GetFaviconForURL.
  if (page_url.SchemeIs(extensions::kExtensionScheme)) {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }
#endif

  if (!content::HasWebUIScheme(page_url))
    return nullptr;

  if (page_url.host_piece() == chrome::kChromeUIComponentsHost)
    return ComponentsUI::GetFaviconResourceBytes(scale_factor);

#if BUILDFLAG(IS_WIN)
  if (page_url.host_piece() == chrome::kChromeUIConflictsHost)
    return ConflictsUI::GetFaviconResourceBytes(scale_factor);
#endif

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  if (page_url.host_piece() == chrome::kChromeUICrashesHost)
    return CrashesUI::GetFaviconResourceBytes(scale_factor);
#endif

  if (page_url.host_piece() == chrome::kChromeUIFlagsHost)
    return FlagsUI::GetFaviconResourceBytes(scale_factor);

#if !BUILDFLAG(IS_ANDROID)
#if !BUILDFLAG(IS_CHROMEOS)
  // The chrome://apps page is not available on Android or ChromeOS.
  if (page_url.host_piece() == chrome::kChromeUIAppLauncherPageHost) {
    return webapps::AppHomeUI::GetFaviconResourceBytes(scale_factor);
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)

  if (page_url.host_piece() == chrome::kChromeUINewTabPageHost)
    return NewTabPageUI::GetFaviconResourceBytes(scale_factor);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  if (page_url.host_piece() == chrome::kChromeUIWhatsNewHost)
    return WhatsNewUI::GetFaviconResourceBytes(scale_factor);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  // Bookmarks are part of NTP on Android.
  if (page_url.host_piece() == chrome::kChromeUIBookmarksHost)
    return BookmarksUI::GetFaviconResourceBytes(scale_factor);

  if (page_url.host_piece() == chrome::kChromeUIHistoryHost)
    return HistoryUI::GetFaviconResourceBytes(scale_factor);

  if (page_url.host_piece() == password_manager::kChromeUIPasswordManagerHost)
    return PasswordManagerUI::GetFaviconResourceBytes(scale_factor);

  // Android uses the native download manager.
  if (page_url.host_piece() == chrome::kChromeUIDownloadsHost)
    return DownloadsUI::GetFaviconResourceBytes(scale_factor);

  // Android doesn't use the Options/Settings pages.
  if (page_url.host_piece() == chrome::kChromeUISettingsHost) {
    return settings_utils::GetFaviconResourceBytes(scale_factor);
  }

  if (page_url.host_piece() == chrome::kChromeUIManagementHost)
    return ManagementUI::GetFaviconResourceBytes(scale_factor);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  if (page_url.host_piece() == commerce::kChromeUICompareHost) {
    return commerce::ProductSpecificationsUI::GetFaviconResourceBytes(
        scale_factor);
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (page_url.host_piece() == chrome::kChromeUIExtensionsHost) {
    return extensions::ExtensionsUI::GetFaviconResourceBytes(scale_factor);
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (page_url.host_piece() == chrome::kChromeUIOSSettingsHost)
    return settings_utils::GetFaviconResourceBytes(scale_factor);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return nullptr;
}

#if BUILDFLAG(IS_CHROMEOS)
const std::vector<GURL>&
ChromeWebUIControllerFactory::GetListOfAcceptableURLs() {
  // clang-format off
  static const base::NoDestructor<std::vector<GURL>> urls({
    // Pages that exist in Ash and in Lacros (separately), with both instances
    // accessible. The Lacros instance is reachable via chrome:// and the Ash
    // instance is reachable via os:// (from Lacros). For convenience and to
    // avoid confusion, the two instances should provide a link to each other.
    GURL(chrome::kChromeUIAboutURL),
    GURL(chrome::kChromeUIAppServiceInternalsURL),
    GURL(chrome::kChromeUIChromeURLsURL),
    GURL(chrome::kChromeUIComponentsUrl),
    GURL(chrome::kChromeUICreditsURL),
    GURL(chrome::kChromeUIDeviceLogUrl),
    GURL(chrome::kChromeUIDlpInternalsURL),
    GURL(chrome::kChromeUIExtensionsInternalsURL),
    GURL(chrome::kChromeUIExtensionsURL),
    GURL(chrome::kChromeUIFlagsURL),
    GURL(chrome::kChromeUIGpuURL),
    GURL(chrome::kChromeUIHistogramsURL),
    GURL(chrome::kChromeUIInspectURL),
    GURL(chrome::kChromeUIManagementURL),
    GURL(chrome::kChromeUINetExportURL),
    GURL(chrome::kChromeUIPrefsInternalsURL),
    GURL(chrome::kChromeUIRestartURL),
    GURL(chrome::kChromeUISignInInternalsUrl),
    GURL(chrome::kChromeUISyncInternalsUrl),
    GURL(chrome::kChromeUISystemURL),
    GURL(chrome::kChromeUITermsURL),
    GURL(chrome::kChromeUIVersionURL),
    GURL(chrome::kChromeUIWebAppInternalsURL),

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Pages that exist only in Ash, i.e. have no immediate counterpart in
    // Lacros. They are reachable via both chrome:// and os:// (from Lacros).
    // Note: chrome://os-settings is also reachable via os://settings.
    GURL(ash::file_manager::kChromeUIFileManagerUntrustedURL),
    GURL(ash::file_manager::kChromeUIFileManagerURL),
    GURL(ash::kChromeUICameraAppURL),
    GURL(ash::kChromeUIFilesInternalsURL),
    GURL(ash::kChromeUIHelpAppURL),
    GURL(ash::kChromeUIMallUrl),
    GURL(ash::kChromeUIPrintPreviewCrosURL),
    GURL(ash::kGrowthInternalsURL),
    GURL(ash::multidevice::kChromeUIProximityAuthURL),
    GURL(ash::kChromeUIRecorderAppURL),
    GURL(ash::vc_background_ui::kChromeUIVcBackgroundURL),
    GURL(chrome::kChromeUIAccountManagerErrorURL),
    GURL(chrome::kChromeUIAccountMigrationWelcomeURL),
    GURL(chrome::kChromeUIAddSupervisionURL),
    GURL(chrome::kChromeUIAppDisabledURL),
    GURL(chrome::kChromeUIArcOverviewTracingURL),
    GURL(chrome::kChromeUIArcPowerControlURL),
    GURL(chrome::kChromeUIAssistantOptInURL),
    GURL(chrome::kChromeUIBluetoothInternalsURL),
    GURL(chrome::kChromeUIBluetoothPairingURL),
    GURL(chrome::kChromeUIBorealisCreditsURL),
    GURL(chrome::kChromeUIBorealisInstallerUrl),
    GURL(chrome::kChromeUICloudUploadURL),
    GURL(chrome::kChromeUILocalFilesMigrationURL),
    GURL(chrome::kChromeUIConnectivityDiagnosticsAppURL),
    GURL(chrome::kChromeUICrashesUrl),
    GURL(chrome::kChromeUICrostiniCreditsURL),
    GURL(chrome::kChromeUICrostiniInstallerUrl),
    GURL(chrome::kChromeUICrostiniUpgraderUrl),
    GURL(chrome::kChromeUICryptohomeURL),
    GURL(chrome::kChromeUIDeviceEmulatorURL),
    GURL(chrome::kChromeUIDiagnosticsAppURL),
    GURL(chrome::kChromeUIDriveInternalsUrl),
    GURL(chrome::kChromeUIEmojiPickerURL),
    GURL(chrome::kChromeUIEnterpriseReportingURL),
    GURL(chrome::kChromeUIFirmwareUpdaterAppURL),
    GURL(chrome::kChromeUIFocusModeMediaURL),
    GURL(chrome::kChromeUIHealthdInternalsURL),
    GURL(chrome::kChromeUIInternetConfigDialogURL),
    GURL(chrome::kChromeUIInternetDetailDialogURL),
    GURL(chrome::kChromeUILauncherInternalsURL),
    GURL(chrome::kChromeUILockScreenNetworkURL),
    GURL(chrome::kChromeUILockScreenStartReauthURL),
    GURL(chrome::kChromeUIManageMirrorSyncURL),
    GURL(chrome::kChromeUIMultiDeviceInternalsURL),
    GURL(chrome::kChromeUIMultiDeviceSetupUrl),
    GURL(chrome::kChromeUINearbyInternalsURL),
    GURL(chrome::kChromeUINetworkUrl),
    GURL(chrome::kChromeUINotificationTesterURL),
    GURL(chrome::kChromeUIOfficeFallbackURL),
    GURL(chrome::kChromeUIOSCreditsURL),
    GURL(chrome::kChromeUIOSSettingsURL),
    GURL(chrome::kChromeUIPowerUrl),
    GURL(chrome::kChromeUIPrintManagementUrl),
    GURL(chrome::kChromeUISanitizeAppURL),
    GURL(chrome::kChromeUIScanningAppURL),
    GURL(chrome::kChromeUISensorInfoURL),
    GURL(chrome::kChromeUISetTimeURL),
    GURL(chrome::kChromeUISlowURL),
    GURL(chrome::kChromeUISmbShareURL),
    GURL(chrome::kChromeUISupportToolURL),
    GURL(chrome::kChromeUISysInternalsUrl),
    GURL(chrome::kChromeUIUntrustedCroshURL),
    GURL(chrome::kChromeUIUntrustedTerminalURL),
    GURL(chrome::kChromeUIUserImageURL),
    GURL(chrome::kChromeUIVmUrl),
    GURL(scalable_iph::kScalableIphDebugURL),

#elif BUILDFLAG(IS_CHROMEOS_LACROS)
    // Pages that only exist in Lacros, where they are reachable via chrome://.
    // TODO(neis): Some of these still exist in Ash (but are inaccessible) and
    // should be removed.
    GURL(chrome::kChromeUIPolicyURL),
    GURL(chrome::kChromeUISettingsURL),
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  });
  // clang-format on
  return *urls;
}

bool ChromeWebUIControllerFactory::CanHandleUrl(const GURL& url) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (url.SchemeIs(extensions::kExtensionScheme) && url.has_host()) {
    std::string extension_id = url.host();
    return extensions::ExtensionRunsInOS(extension_id);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return crosapi::gurl_os_handler_utils::IsAshUrlInList(
      url, GetListOfAcceptableURLs());
}

#endif  // BUILDFLAG(IS_CHROMEOS)
