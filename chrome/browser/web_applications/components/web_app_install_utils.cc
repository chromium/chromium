// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_install_utils.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "chrome/browser/banners/app_banner_manager.h"
#include "chrome/browser/banners/app_banner_manager_desktop.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/installable/installable_data.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/common/web_application_info.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace web_app {

namespace {

// Get a list of non-empty square icons from |web_app_info|.
void FilterSquareIconsFromInfo(const WebApplicationInfo& web_app_info,
                               std::vector<BitmapAndSource>* square_icons) {
  // Add all existing icons from WebApplicationInfo.
  for (const WebApplicationIconInfo& icon_info : web_app_info.icons) {
    const SkBitmap& icon = icon_info.data;
    if (!icon.drawsNothing() && icon.width() == icon.height())
      square_icons->push_back(BitmapAndSource(icon_info.url, icon));
  }
}

// Get a list of non-empty square icons from |icons_map|.
void FilterSquareIconsFromMap(const IconsMap& icons_map,
                              std::vector<BitmapAndSource>* square_icons) {
  for (const std::pair<GURL, std::vector<SkBitmap>>& url_icon : icons_map) {
    for (const SkBitmap& icon : url_icon.second) {
      if (!icon.empty() && icon.width() == icon.height())
        square_icons->push_back(BitmapAndSource(url_icon.first, icon));
    }
  }
}

// This function replaces |web_app_info| icons with the image data of any icon
// from |size_map|.
void ReplaceWebAppIcons(std::map<int, BitmapAndSource> size_map,
                        WebApplicationInfo* web_app_info) {
  web_app_info->icons.clear();

  // Populate the icon data into the WebApplicationInfo we are using to
  // install the bookmark app.
  for (const auto& size_and_icon : size_map) {
    WebApplicationIconInfo icon_info;
    icon_info.data = size_and_icon.second.bitmap;
    icon_info.url = size_and_icon.second.source_url;
    icon_info.width = icon_info.data.width();
    icon_info.height = icon_info.data.height();
    web_app_info->icons.push_back(icon_info);
  }
}

// This function updates |web_app_info| with the image data of any icon from
// |size_map| that has a URL and size matching that in |web_app_info|, as
// well as adding any new images from |size_map| that have no URL.
void UpdateWebAppIconsWithoutChangingLinks(
    const std::map<int, BitmapAndSource>& size_map,
    WebApplicationInfo* web_app_info) {
  // First add in the icon data that have urls with the url / size data from the
  // original web app info, and the data from the new icons (if any).
  for (auto& icon : web_app_info->icons) {
    if (!icon.url.is_empty() && icon.data.empty()) {
      const auto& it = size_map.find(icon.width);
      if (it != size_map.end() && it->second.source_url == icon.url) {
        icon.height = icon.width;
        icon.data = it->second.bitmap;
      }
    }
  }

  // Now add in any icons from the updated list that don't have URLs.
  for (const auto& pair : size_map) {
    if (pair.second.source_url.is_empty()) {
      WebApplicationIconInfo icon_info;
      icon_info.data = pair.second.bitmap;
      icon_info.width = pair.first;
      icon_info.height = pair.first;
      web_app_info->icons.push_back(icon_info);
    }
  }
}

}  // namespace

void UpdateWebAppInfoFromManifest(const blink::Manifest& manifest,
                                  WebApplicationInfo* web_app_info,
                                  ForInstallableSite for_installable_site) {
  if (!manifest.short_name.is_null())
    web_app_info->title = manifest.short_name.string();

  // Give the full length name priority.
  if (!manifest.name.is_null())
    web_app_info->title = manifest.name.string();

  // Set the url based on the manifest value, if any.
  if (manifest.start_url.is_valid())
    web_app_info->app_url = manifest.start_url;

  if (for_installable_site == ForInstallableSite::kYes)
    web_app_info->scope = manifest.scope;

  if (manifest.theme_color)
    web_app_info->theme_color = *manifest.theme_color;

  // When the display member is missing, or if there is no valid display member,
  // the user agent uses the browser display mode as the default display mode.
  // https://w3c.github.io/manifest/#display-modes
  web_app_info->display_mode = (manifest.display == DisplayMode::kUndefined)
                                   ? DisplayMode::kBrowser
                                   : manifest.display;

  // Create the WebApplicationInfo icons list *outside* of |web_app_info|, so
  // that we can decide later whether or not to replace the existing icons array
  // (conditionally on whether there were any that didn't have purpose ANY).
  std::vector<WebApplicationIconInfo> web_app_icons;
  for (const auto& icon : manifest.icons) {
    // An icon's purpose vector should never be empty (the manifest parser
    // should have added ANY if there was no purpose specified in the manifest).
    DCHECK(!icon.purpose.empty());

    if (!base::Contains(icon.purpose,
                        blink::Manifest::ImageResource::Purpose::ANY)) {
      continue;
    }

    // TODO(benwells): Take the declared icon density and sizes into account.
    WebApplicationIconInfo info;
    info.url = icon.src;
    web_app_icons.push_back(info);
  }

  // If any icons are specified in the manifest, they take precedence over any
  // we picked up from the web_app stuff.
  if (!web_app_icons.empty())
    web_app_info->icons = std::move(web_app_icons);

  // Copy across the file handler info.
  web_app_info->file_handler = manifest.file_handler;
}

std::vector<GURL> GetValidIconUrlsToDownload(
    const WebApplicationInfo& web_app_info) {
  std::vector<GURL> web_app_info_icon_urls;
  for (auto& info : web_app_info.icons) {
    if (!info.url.is_valid())
      continue;
    web_app_info_icon_urls.push_back(info.url);
  }
  return web_app_info_icon_urls;
}

void FilterAndResizeIconsGenerateMissing(WebApplicationInfo* web_app_info,
                                         const IconsMap* icons_map,
                                         bool is_for_sync) {
  // Ensure that all icons that are in web_app_info are present, by generating
  // icons for any sizes which have failed to download. This ensures that the
  // created manifest for the web app does not contain links to icons
  // which are not actually created and linked on disk.
  std::vector<BitmapAndSource> square_icons;
  if (icons_map)
    FilterSquareIconsFromMap(*icons_map, &square_icons);
  if (!is_for_sync)
    FilterSquareIconsFromInfo(*web_app_info, &square_icons);

  std::set<int> sizes_to_generate = SizesToGenerate();
  if (is_for_sync) {
    // Ensure that all icon widths in the web app info icon array are present in
    // the sizes to generate set. This ensures that we will have all of the
    // icon sizes from when the app was originally added, even if icon URLs are
    // no longer accessible.
    for (const auto& icon : web_app_info->icons)
      sizes_to_generate.insert(icon.width);
  }

  web_app_info->generated_icon_color = SK_ColorTRANSPARENT;
  std::map<int, BitmapAndSource> size_to_icon = ResizeIconsAndGenerateMissing(
      square_icons, sizes_to_generate, web_app_info->app_url,
      &web_app_info->generated_icon_color);

  if (is_for_sync)
    UpdateWebAppIconsWithoutChangingLinks(size_to_icon, web_app_info);
  else
    ReplaceWebAppIcons(size_to_icon, web_app_info);
}

void RecordAppBanner(content::WebContents* contents, const GURL& app_url) {
  AppBannerSettingsHelper::RecordBannerEvent(
      contents, app_url, app_url.spec(),
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN,
      base::Time::Now());
}

WebappInstallSource ConvertExternalInstallSourceToInstallSource(
    ExternalInstallSource external_install_source) {
  WebappInstallSource install_source;
  switch (external_install_source) {
    case ExternalInstallSource::kInternalDefault:
      install_source = WebappInstallSource::INTERNAL_DEFAULT;
      break;
    case ExternalInstallSource::kExternalDefault:
      install_source = WebappInstallSource::EXTERNAL_DEFAULT;
      break;
    case ExternalInstallSource::kExternalPolicy:
      install_source = WebappInstallSource::EXTERNAL_POLICY;
      break;
    case ExternalInstallSource::kSystemInstalled:
      install_source = WebappInstallSource::SYSTEM_DEFAULT;
      break;
    case ExternalInstallSource::kArc:
      install_source = WebappInstallSource::ARC;
      break;
  }

  return install_source;
}

void RecordExternalAppInstallResultCode(
    const char* histogram_name,
    std::map<GURL, InstallResultCode> install_results) {
  for (const auto& url_and_result : install_results)
    base::UmaHistogramEnumeration(histogram_name, url_and_result.second);
}

}  // namespace web_app
