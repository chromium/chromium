// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/favicon_source.h"

#include <cmath>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/history_ui_favicon_request_handler_factory.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_io_context.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/favicon/core/history_ui_favicon_request_handler.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/history/core/browser/top_sites.h"
#include "content/public/browser/web_contents.h"
#include "net/url_request/url_request.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"
#include "url/gurl.h"

namespace {

// web_contents->GetLastCommittedURL in general will not necessarily yield the
// original URL that started the request, but we're only interested in verifying
// if it was issued by a history page, for whom this is the case. If it is not
// possible to obtain the URL, we return the empty GURL.
GURL GetUnsafeRequestOrigin(const content::WebContents::Getter& wc_getter) {
  content::WebContents* web_contents = wc_getter.Run();
  return web_contents ? web_contents->GetLastCommittedURL() : GURL();
}

bool ParseHistoryUiOrigin(const GURL& url,
                          favicon::HistoryUiFaviconRequestOrigin* origin) {
  GURL history_url(chrome::kChromeUIHistoryURL);
  if (url == history_url) {
    *origin = favicon::HistoryUiFaviconRequestOrigin::kHistory;
    return true;
  }
  if (url == history_url.Resolve(chrome::kChromeUIHistorySyncedTabs)) {
    *origin = favicon::HistoryUiFaviconRequestOrigin::kHistorySyncedTabs;
    return true;
  }
  return false;
}

}  // namespace

FaviconSource::FaviconSource(Profile* profile,
                             chrome::FaviconUrlFormat url_format)
    : profile_(profile->GetOriginalProfile()), url_format_(url_format) {}

FaviconSource::~FaviconSource() {
}

std::string FaviconSource::GetSource() {
  switch (url_format_) {
    case chrome::FaviconUrlFormat::kFaviconLegacy:
      return chrome::kChromeUIFaviconHost;
    case chrome::FaviconUrlFormat::kFavicon2:
      return chrome::kChromeUIFavicon2Host;
  }
  NOTREACHED();
  return "";
}

void FaviconSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    const content::URLDataSource::GotDataCallback& callback) {
  const std::string path = content::URLDataSource::URLToRequestPath(url);
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  if (!favicon_service) {
    SendDefaultResponse(callback);
    return;
  }

  chrome::ParsedFaviconPath parsed;
  bool success = chrome::ParseFaviconPath(path, url_format_, &parsed);
  if (!success) {
    SendDefaultResponse(callback);
    return;
  }

  GURL page_url(parsed.page_url);
  GURL icon_url(parsed.icon_url);
  if (!page_url.is_valid() && !icon_url.is_valid()) {
    SendDefaultResponse(callback);
    return;
  }

  int desired_size_in_pixel =
      std::ceil(parsed.size_in_dip * parsed.device_scale_factor);

  if (parsed.page_url.empty()) {
    // Request by icon url.

    // TODO(michaelbai): Change GetRawFavicon to support combination of
    // IconType.
    favicon_service->GetRawFavicon(
        icon_url, favicon_base::IconType::kFavicon, desired_size_in_pixel,
        base::BindRepeating(&FaviconSource::OnFaviconDataAvailable,
                            base::Unretained(this), callback,
                            parsed.size_in_dip, parsed.device_scale_factor),
        &cancelable_task_tracker_);
  } else {
    // Intercept requests for prepopulated pages if TopSites exists.
    scoped_refptr<history::TopSites> top_sites =
        TopSitesFactory::GetForProfile(profile_);
    if (top_sites) {
      for (const auto& prepopulated_page : top_sites->GetPrepopulatedPages()) {
        if (page_url == prepopulated_page.most_visited.url) {
          ui::ScaleFactor resource_scale_factor =
              ui::GetSupportedScaleFactor(parsed.device_scale_factor);
          callback.Run(
              ui::ResourceBundle::GetSharedInstance()
                  .LoadDataResourceBytesForScale(prepopulated_page.favicon_id,
                                                 resource_scale_factor));
          return;
        }
      }
    }

    favicon::HistoryUiFaviconRequestOrigin parsed_history_ui_origin;
    if (!parsed.allow_favicon_server_fallback ||
        !ParseHistoryUiOrigin(GetUnsafeRequestOrigin(wc_getter),
                              &parsed_history_ui_origin)) {
      // Request from local storage only.
      // TODO(victorvianna): Expose fallback_to_host in FaviconRequestHandler
      // API and move the explanatory comment for |fallback_to_host| here.
      const bool fallback_to_host = true;
      favicon_service->GetRawFaviconForPageURL(
          page_url, {favicon_base::IconType::kFavicon}, desired_size_in_pixel,
          fallback_to_host,
          base::Bind(&FaviconSource::OnFaviconDataAvailable,
                     base::Unretained(this), callback, parsed.size_in_dip,
                     parsed.device_scale_factor),
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
      SendDefaultResponse(callback);
      return;
    }
    history_ui_favicon_request_handler->GetRawFaviconForPageURL(
        page_url, desired_size_in_pixel,
        base::BindOnce(&FaviconSource::OnFaviconDataAvailable,
                       base::Unretained(this), callback, parsed.size_in_dip,
                       parsed.device_scale_factor),
        favicon::FaviconRequestPlatform::kDesktop, parsed_history_ui_origin,
        /*icon_url_for_uma=*/
        GURL(parsed.icon_url), &cancelable_task_tracker_);
  }
}

std::string FaviconSource::GetMimeType(const std::string&) {
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
    content::ResourceContext* resource_context,
    int render_process_id) {
  if (url.SchemeIs(chrome::kChromeSearchScheme)) {
    return InstantIOContext::ShouldServiceRequest(url, resource_context,
                                                  render_process_id);
  }
  return URLDataSource::ShouldServiceRequest(url, resource_context,
                                             render_process_id);
}

ui::NativeTheme* FaviconSource::GetNativeTheme() {
  return ui::NativeTheme::GetInstanceForNativeUi();
}

void FaviconSource::OnFaviconDataAvailable(
    const content::URLDataSource::GotDataCallback& callback,
    int size_in_dip,
    float scale_factor,
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  if (bitmap_result.is_valid()) {
    // Forward the data along to the networking system.
    callback.Run(bitmap_result.bitmap_data.get());
  } else {
    SendDefaultResponse(callback, size_in_dip, scale_factor);
  }
}

void FaviconSource::SendDefaultResponse(
    const content::URLDataSource::GotDataCallback& callback) {
  SendDefaultResponse(callback, 16, 1.0f);
}

void FaviconSource::SendDefaultResponse(
    const content::URLDataSource::GotDataCallback& callback,
    int size_in_dip,
    float scale_factor) {
  const bool dark = GetNativeTheme()->ShouldUseDarkColors();
  int resource_id;
  switch (size_in_dip) {
    case 64:
      resource_id = dark ? IDR_DEFAULT_FAVICON_DARK_64 : IDR_DEFAULT_FAVICON_64;
      break;
    case 32:
      resource_id = dark ? IDR_DEFAULT_FAVICON_DARK_32 : IDR_DEFAULT_FAVICON_32;
      break;
    default:
      resource_id = dark ? IDR_DEFAULT_FAVICON_DARK : IDR_DEFAULT_FAVICON;
      break;
  }
  callback.Run(LoadIconBytes(scale_factor, resource_id));
}

base::RefCountedMemory* FaviconSource::LoadIconBytes(float scale_factor,
                                                     int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      resource_id, ui::GetSupportedScaleFactor(scale_factor));
}
