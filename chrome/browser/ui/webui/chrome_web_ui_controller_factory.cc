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
#include "build/android_buildflags.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/about/about_ui.h"
#include "chrome/browser/ui/webui/components/components_ui.h"
#include "chrome/browser/ui/webui/crashes/crashes_ui.h"
#include "chrome/browser/ui/webui/download_internals/download_internals_ui.h"
#include "chrome/browser/ui/webui/flags/flags_ui.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/product_specifications/product_specifications_set.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_util.h"
#include "components/favicon_base/select_favicon_frames.h"
#include "components/grit/components_scaled_resources.h"
#include "components/history/core/browser/history_types.h"
#include "components/password_manager/content/common/web_ui_constants.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
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
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/app_service_internals/app_service_internals_ui.h"
#include "chrome/browser/ui/webui/bookmarks/bookmarks_ui.h"
#include "chrome/browser/ui/webui/downloads/downloads_ui.h"
#include "chrome/browser/ui/webui/history/history_ui.h"
#include "chrome/browser/ui/webui/management/management_ui.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "chrome/browser/ui/webui/password_manager/password_manager_ui.h"
#include "chrome/browser/ui/webui/settings/settings_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "media/base/media_switches.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia_rep.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_DEVTOOLS_FRONTEND)
#include "chrome/browser/devtools/devtools_ui_bindings.h"
#include "chrome/browser/ui/webui/devtools/devtools_ui.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
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
#include "chromeos/ash/components/scalable_iph/scalable_iph_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/app_home/app_home_ui.h"
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
#include "extensions/browser/extension_registry.h"  // nogncheck
#include "extensions/browser/extension_system.h"    // nogncheck
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/ui/webui/extensions/extensions_ui.h"
#endif

using content::WebUI;
using content::WebUIController;
using ui::WebDialogUI;

namespace {

// A function for creating a new WebUI. The caller owns the return value, which
// may be nullptr (for example, if the URL refers to an non-existent extension).
typedef WebUIController* (*WebUIFactoryFunction)(WebUI* web_ui,
                                                 const GURL& url);

// Template for defining WebUIFactoryFunction.
template <class T>
WebUIController* NewWebUI(WebUI* web_ui, const GURL& url) {
  return new T(web_ui);
}

// Returns a function that can be used to create the right type of WebUI for a
// tab, based on its URL. Returns nullptr if the URL doesn't have WebUI
// associated with it.
WebUIFactoryFunction GetWebUIFactoryFunction(WebUI* web_ui,
                                             Profile* profile,
                                             const GURL& url) {
  // This will get called a lot to check all URLs, so do a quick check of other
  // schemes to filter out most URLs.
#if BUILDFLAG(ENABLE_DEVTOOLS_FRONTEND)
  if (!url.SchemeIs(content::kChromeDevToolsScheme)) {
    return nullptr;
  }
  if (!DevToolsUIBindings::IsValidFrontendURL(url)) {
    return nullptr;
  }
  return &NewWebUI<DevToolsUI>;
#else
  return nullptr;
#endif  // BUILDFLAG(ENABLE_DEVTOOLS_FRONTEND)
}

#if !BUILDFLAG(IS_ANDROID)
// Reads favicons for the IWA represented by `page_url` in all available sizes
// from the disk.
// `callback` is always run asynchronously for consistency with how extensions
// favicons are read.
void ReadIsolatedWebAppFaviconsFromDisk(
    Profile* profile,
    const GURL& page_url,
    favicon_base::FaviconResultsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto callback_async = base::BindOnce(
      [](favicon_base::FaviconResultsCallback callback,
         std::vector<favicon_base::FaviconRawBitmapResult>
             favicon_bitmap_results) {
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback),
                                      std::move(favicon_bitmap_results)));
      },
      std::move(callback));

  auto* web_app_provider = web_app::WebAppProvider::GetForWebApps(profile);
  if (!web_app_provider) {
    std::move(callback_async).Run(/*favicon_bitmap_results=*/{});
    return;
  }

  ASSIGN_OR_RETURN(
      auto url_info, web_app::IsolatedWebAppUrlInfo::Create(page_url),
      [&](auto) {
        std::move(callback_async).Run(/*favicon_bitmap_results=*/{});
      });

  web_app_provider->icon_manager().ReadFavicons(
      url_info.app_id(), /*purpose=*/web_app::IconPurpose::ANY,
      base::BindOnce([](gfx::ImageSkia image_skia) {
        std::vector<favicon_base::FaviconRawBitmapResult>
            favicon_bitmap_results;

        for (const gfx::ImageSkiaRep& image_rep : image_skia.image_reps()) {
          if (std::optional<std::vector<uint8_t>> data =
                  gfx::PNGCodec::EncodeBGRASkBitmap(
                      image_rep.GetBitmap(), /*discard_transparency=*/false)) {
            favicon_base::FaviconRawBitmapResult bitmap_result;
            bitmap_result.bitmap_data =
                base::MakeRefCounted<base::RefCountedBytes>(std::move(*data));
            bitmap_result.pixel_size =
                gfx::Size(image_rep.pixel_width(), image_rep.pixel_height());
            // Leave |bitmap_result|'s icon URL as the default of GURL().
            bitmap_result.icon_type = favicon_base::IconType::kFavicon;

            favicon_bitmap_results.push_back(std::move(bitmap_result));
          }
        }

        return favicon_bitmap_results;
      }).Then(std::move(callback_async)));
}
#endif

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
  if (!function) {
    return nullptr;
  }

  return base::WrapUnique((*function)(web_ui, url));
}

void ChromeWebUIControllerFactory::GetFaviconForURL(
    Profile* profile,
    const GURL& page_url,
    const std::vector<int>& desired_sizes_in_pixel,
    favicon_base::FaviconResultsCallback callback) const {
#if !BUILDFLAG(IS_ANDROID)
  if (page_url.SchemeIs(chrome::kIsolatedAppScheme)) {
    ReadIsolatedWebAppFaviconsFromDisk(profile, page_url, std::move(callback));
    return;
  }
#endif

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
    candidate_sizes.emplace_back(candidate_edge_size, candidate_edge_size);
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
      origin.host() == chrome::kChromeUIDownloadsHost ||
      // https://crbug.com/376417346
      origin.host() == chrome::kChromeUIExtensionsHost;
}

ChromeWebUIControllerFactory::ChromeWebUIControllerFactory() = default;

ChromeWebUIControllerFactory::~ChromeWebUIControllerFactory() = default;

base::RefCountedMemory* ChromeWebUIControllerFactory::GetFaviconResourceBytes(
    const GURL& page_url,
    ui::ResourceScaleFactor scale_factor) const {
#if !BUILDFLAG(IS_ANDROID)
  // The extension scheme is handled in GetFaviconForURL.
  if (page_url.SchemeIs(extensions::kExtensionScheme)) {
    NOTREACHED();
  }
#endif

  if (!content::HasWebUIScheme(page_url)) {
    return nullptr;
  }

  if (page_url.host_piece() == chrome::kChromeUIComponentsHost) {
    return ComponentsUI::GetFaviconResourceBytes(scale_factor);
  }

#if BUILDFLAG(IS_WIN)
  if (page_url.host_piece() == chrome::kChromeUIConflictsHost) {
    return ConflictsUI::GetFaviconResourceBytes(scale_factor);
  }
#endif

  if (page_url.host_piece() == chrome::kChromeUICrashesHost) {
    return CrashesUI::GetFaviconResourceBytes(scale_factor);
  }

  if (page_url.host_piece() == chrome::kChromeUIFlagsHost) {
    return FlagsUI::GetFaviconResourceBytes(scale_factor);
  }

#if !BUILDFLAG(IS_ANDROID)
#if !BUILDFLAG(IS_CHROMEOS)
  // The chrome://apps page is not available on Android or ChromeOS.
  if (page_url.host_piece() == chrome::kChromeUIAppLauncherPageHost) {
    return webapps::AppHomeUI::GetFaviconResourceBytes(scale_factor);
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)

  if (page_url.host_piece() == chrome::kChromeUINewTabPageHost) {
    return NewTabPageUI::GetFaviconResourceBytes(scale_factor);
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  if (page_url.host_piece() == chrome::kChromeUIWhatsNewHost) {
    return WhatsNewUI::GetFaviconResourceBytes(scale_factor);
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  // Bookmarks are part of NTP on Android.
  if (page_url.host_piece() == chrome::kChromeUIBookmarksHost) {
    return BookmarksUI::GetFaviconResourceBytes(scale_factor);
  }

  if (page_url.host_piece() == chrome::kChromeUIHistoryHost) {
    return HistoryUI::GetFaviconResourceBytes(scale_factor);
  }

  if (page_url.host_piece() == password_manager::kChromeUIPasswordManagerHost) {
    return PasswordManagerUI::GetFaviconResourceBytes(scale_factor);
  }

  // Android uses the native download manager.
  if (page_url.host_piece() == chrome::kChromeUIDownloadsHost) {
    return DownloadsUI::GetFaviconResourceBytes(scale_factor);
  }

  // Android doesn't use the Options/Settings pages.
  if (page_url.host_piece() == chrome::kChromeUISettingsHost) {
    return settings_utils::GetFaviconResourceBytes(scale_factor);
  }

  if (page_url.host_piece() == chrome::kChromeUIManagementHost) {
    return ManagementUI::GetFaviconResourceBytes(scale_factor);
  }

  // Tab Search hosts the split tab NTP and so leveraging the NTP favicon.
  if (page_url.host_piece() == chrome::kChromeUITabSearchHost) {
    return NewTabPageUI::GetFaviconResourceBytes(scale_factor);
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  if (page_url.host_piece() == commerce::kChromeUICompareHost) {
    return commerce::ProductSpecificationsUI::GetFaviconResourceBytes(
        scale_factor);
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (page_url.host_piece() == chrome::kChromeUIExtensionsHost) {
    return extensions::ExtensionsUI::GetFaviconResourceBytes(scale_factor);
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

#if BUILDFLAG(IS_CHROMEOS)
  if (page_url.host_piece() == chrome::kChromeUIOSSettingsHost) {
    return settings_utils::GetFaviconResourceBytes(scale_factor);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  return nullptr;
}
