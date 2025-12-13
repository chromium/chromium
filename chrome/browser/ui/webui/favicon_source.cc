// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/favicon_source.h"

#include <cmath>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/favicon/history_ui_favicon_request_handler_factory.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/favicon/core/history_ui_favicon_request_handler.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/history/core/browser/top_sites.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/ui/webui/webui_util_desktop.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace {

// Name of histogram to track whether the default response was returned.
const char kDefaultResponseHistogramName[] = "Favicons.DefaultResponse";

// Generous cap to guard against out-of-memory issues.
constexpr int kMaxDesiredSizeInPixel = 2048;

// web_contents->GetLastCommittedURL in general will not necessarily yield the
// original URL that started the request, but we're only interested in verifying
// if it was issued by a history page, for whom this is the case. If it is not
// possible to obtain the URL, we return the empty GURL.
GURL GetUnsafeRequestOrigin(const content::WebContents::Getter& wc_getter) {
  content::WebContents* web_contents = wc_getter.Run();
  return web_contents ? web_contents->GetLastCommittedURL() : GURL();
}

bool IsOriginAllowedServerFallback(const GURL& url) {
  // Allow chrome-untrusted://data-sharing to use Google server fallback.
  if (url.GetScheme() == content::kChromeUIUntrustedScheme &&
      url.GetHost() == chrome::kChromeUIUntrustedDataSharingHost) {
    return true;
  }
  GURL history_url(chrome::kChromeUIHistoryURL);
  if (url == history_url) {
    return true;
  }
  if (url == history_url.Resolve(chrome::kChromeUIHistorySyncedTabs)) {
    return true;
  }
  if (url == GURL(chrome::kChromeUINewTabURL) ||
      url == GURL(chrome::kChromeUINewTabPageURL)) {
    return true;
  }
  return false;
}

}  // namespace

FaviconSource::FaviconSource(Profile* profile,
                             chrome::FaviconUrlFormat url_format,
                             bool serve_untrusted)
    : profile_(profile->GetOriginalProfile()),
      url_format_(url_format),
      serve_untrusted_(serve_untrusted) {}

FaviconSource::~FaviconSource() = default;

std::string FaviconSource::GetSource() {
  switch (url_format_) {
    case chrome::FaviconUrlFormat::kFaviconLegacy:
      return chrome::kChromeUIFaviconHost;
    case chrome::FaviconUrlFormat::kFavicon2:
      return serve_untrusted_ ? chrome::kChromeUIUntrustedFavicon2URL
                              : chrome::kChromeUIFavicon2Host;
  }

  NOTREACHED();
}

void FaviconSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const std::string path = content::URLDataSource::URLToRequestPath(url);
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  if (!favicon_service) {
    SendDefaultResponse(std::move(callback), wc_getter);
    return;
  }

  chrome::ParsedFaviconPath parsed;
  bool success = chrome::ParseFaviconPath(path, url_format_, &parsed);
  if (!success) {
    SendDefaultResponse(std::move(callback), wc_getter);
    return;
  }

  const auto default_favicon_behavior =
      parsed.force_empty_default_favicon
          ? DefaultFaviconBehavior::kUseEmptyIcon
          : DefaultFaviconBehavior::kUseGlobeIcon;

  GURL page_url(parsed.page_url);
  GURL icon_url(parsed.icon_url);
  if (!page_url.is_valid() && !icon_url.is_valid()) {
    SendDefaultResponse(std::move(callback), wc_getter, parsed.force_light_mode,
                        default_favicon_behavior);
    return;
  }

  const int desired_size_in_pixel =
      std::ceil(parsed.size_in_dip * parsed.device_scale_factor);

  // Guard against out-of-memory issues.
  if (desired_size_in_pixel > kMaxDesiredSizeInPixel) {
    SendDefaultResponse(std::move(callback), wc_getter, parsed.force_light_mode,
                        default_favicon_behavior);
    return;
  }

  if (parsed.page_url.empty()) {
    // Request by icon url.

    base::UmaHistogramBoolean(kDefaultResponseHistogramName, false);
    // TODO(michaelbai): Change GetRawFavicon to support combination of
    // IconType.
    favicon_service->GetRawFavicon(
        icon_url, favicon_base::IconType::kFavicon, desired_size_in_pixel,
        base::BindOnce(&FaviconSource::OnFaviconDataAvailable,
                       base::Unretained(this), std::move(callback), parsed,
                       wc_getter),
        &cancelable_task_tracker_);
  } else {
    // Intercept requests for prepopulated pages if TopSites exists.
    scoped_refptr<history::TopSites> top_sites =
        TopSitesFactory::GetForProfile(profile_);
    if (top_sites) {
      for (const auto& prepopulated_page : top_sites->GetPrepopulatedPages()) {
        if (page_url == prepopulated_page.most_visited.url) {
          base::UmaHistogramBoolean(kDefaultResponseHistogramName, false);
          ui::ResourceScaleFactor resource_scale_factor =
              ui::GetSupportedResourceScaleFactor(parsed.device_scale_factor);
          std::move(callback).Run(
              ui::ResourceBundle::GetSharedInstance()
                  .LoadDataResourceBytesForScale(prepopulated_page.favicon_id,
                                                 resource_scale_factor));
          return;
        }
      }
    }

    if (!(parsed.allow_favicon_server_fallback &&
          IsOriginAllowedServerFallback(GetUnsafeRequestOrigin(wc_getter)))) {
      base::UmaHistogramBoolean(kDefaultResponseHistogramName, false);
      // Request from local storage only.
      const bool fallback_to_host = true;
      favicon_service->GetRawFaviconForPageURL(
          page_url, {favicon_base::IconType::kFavicon}, desired_size_in_pixel,
          fallback_to_host,
          base::BindOnce(&FaviconSource::OnFaviconDataAvailable,
                         base::Unretained(this), std::move(callback), parsed,
                         wc_getter),
          &cancelable_task_tracker_);
      return;
    }

    // Request from both local storage and favicon server using
    // HistoryUiFaviconRequestHandler.
    favicon::HistoryUiFaviconRequestHandler*
        history_ui_favicon_request_handler =
            HistoryUiFaviconRequestHandlerFactory::GetForBrowserContext(
                profile_);
    if (!history_ui_favicon_request_handler) {
      SendDefaultResponse(std::move(callback), parsed, wc_getter);
      return;
    }
    base::UmaHistogramBoolean(kDefaultResponseHistogramName, false);
    history_ui_favicon_request_handler->GetRawFaviconForPageURL(
        page_url, desired_size_in_pixel, parsed.fallback_to_host,
        base::BindOnce(&FaviconSource::OnFaviconDataAvailable,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       parsed, wc_getter));
  }
}

std::string FaviconSource::GetMimeType(const GURL&) {
  // We need to explicitly return a mime type, otherwise if the user tries to
  // drag the image they get no extension.
  return "image/png";
}

bool FaviconSource::AllowCaching() {
  return false;
}

bool FaviconSource::ShouldReplaceExistingSource() {
  // Leave the existing DataSource in place, otherwise we'll drop any pending
  // requests on the floor.
  return false;
}

bool FaviconSource::ShouldServiceRequest(
    const GURL& url,
    content::BrowserContext* browser_context,
    int render_process_id) {
#if !BUILDFLAG(IS_ANDROID)
  if (url.SchemeIs(chrome::kChromeSearchScheme)) {
    return InstantService::ShouldServiceRequest(url, browser_context,
                                                render_process_id);
  }
#endif
  return URLDataSource::ShouldServiceRequest(url, browser_context,
                                             render_process_id);
}

ui::NativeTheme* FaviconSource::GetNativeTheme(
    const content::WebContents::Getter& wc_getter) {
#if BUILDFLAG(IS_ANDROID)
  return ui::NativeTheme::GetInstanceForNativeUi();
#else
  return webui::GetNativeThemeDeprecated(wc_getter.Run());
#endif
}

void FaviconSource::OnFaviconDataAvailable(
    content::URLDataSource::GotDataCallback callback,
    const chrome::ParsedFaviconPath& parsed,
    const content::WebContents::Getter& wc_getter,
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  if (bitmap_result.is_valid()) {
    // Forward the data along to the networking system.
    std::move(callback).Run(bitmap_result.bitmap_data.get());
  } else {
    SendDefaultResponse(std::move(callback), parsed, wc_getter);
  }
}

void FaviconSource::SendDefaultResponse(
    content::URLDataSource::GotDataCallback callback,
    const chrome::ParsedFaviconPath& parsed,
    const content::WebContents::Getter& wc_getter) {
  if (!parsed.show_fallback_monogram) {
    SendDefaultResponse(
        std::move(callback), parsed.size_in_dip, parsed.device_scale_factor,
        !parsed.force_light_mode &&
            GetNativeTheme(wc_getter)->preferred_color_scheme() ==
                ui::NativeTheme::PreferredColorScheme::kDark,
        parsed.force_empty_default_favicon
            ? DefaultFaviconBehavior::kUseEmptyIcon
            : DefaultFaviconBehavior::kUseGlobeIcon);
    return;
  }
  int icon_size = std::ceil(parsed.size_in_dip * parsed.device_scale_factor);
  SkBitmap bitmap = favicon::GenerateMonogramFavicon(GURL(parsed.page_url),
                                                     icon_size, icon_size);
  std::optional<std::vector<uint8_t>> bitmap_data =
      gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, /*discard_transparency=*/false);
  DCHECK(bitmap_data);
  std::move(callback).Run(base::MakeRefCounted<base::RefCountedBytes>(
      std::move(bitmap_data).value()));
}

void FaviconSource::SendDefaultResponse(
    content::URLDataSource::GotDataCallback callback,
    const content::WebContents::Getter& wc_getter,
    bool force_light_mode,
    DefaultFaviconBehavior behavior) {
  SendDefaultResponse(std::move(callback), 16, 1.0f,
                      !force_light_mode &&
                          GetNativeTheme(wc_getter)->preferred_color_scheme() ==
                              ui::NativeTheme::PreferredColorScheme::kDark,
                      behavior);
}

void FaviconSource::SendDefaultResponse(
    content::URLDataSource::GotDataCallback callback,
    int size_in_dip,
    float scale_factor,
    bool dark_mode,
    DefaultFaviconBehavior behavior) {
  base::UmaHistogramBoolean(kDefaultResponseHistogramName, true);

  if (behavior == DefaultFaviconBehavior::kUseEmptyIcon) {
    std::move(callback).Run(nullptr);
    return;
  }

  int resource_id;
  switch (size_in_dip) {
    case 64:
      resource_id =
          dark_mode ? IDR_DEFAULT_FAVICON_DARK_64 : IDR_DEFAULT_FAVICON_64;
      break;
    case 32:
      resource_id =
          dark_mode ? IDR_DEFAULT_FAVICON_DARK_32 : IDR_DEFAULT_FAVICON_32;
      break;
    default:
      resource_id = dark_mode ? IDR_DEFAULT_FAVICON_DARK : IDR_DEFAULT_FAVICON;
      break;
  }
  std::move(callback).Run(LoadIconBytes(scale_factor, resource_id));
}

base::RefCountedMemory* FaviconSource::LoadIconBytes(float scale_factor,
                                                     int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      resource_id, ui::GetSupportedResourceScaleFactor(scale_factor));
}
