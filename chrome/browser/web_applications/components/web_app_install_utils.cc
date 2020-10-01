// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_install_utils.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "chrome/browser/banners/app_banner_manager.h"
#include "chrome/browser/banners/app_banner_manager_desktop.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/installable/installable_data.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace web_app {

namespace {

// We restrict the number of icons to limit disk usage per installed PWA. This
// value can change overtime as new features are added.
constexpr int kMaxIcons = 20;
constexpr SquareSizePx kMaxIconSize = 1024;

// Append non-empty square icons from |icons_map| onto the |square_icons| list.
void AddSquareIconsFromMap(std::vector<SkBitmap>* square_icons,
                           const IconsMap& icons_map) {
  for (const auto& url_icon : icons_map) {
    for (const SkBitmap& icon : url_icon.second) {
      if (!icon.empty() && icon.width() == icon.height())
        square_icons->push_back(icon);
    }
  }
}

// Append non-empty square icons from |icons_map| onto the |square_icons| list,
// if they are also in |icon_infos|.
void AddSquareIconsFromMapMatchingIconInfos(
    std::vector<SkBitmap>* square_icons,
    const std::vector<WebApplicationIconInfo>& icon_infos,
    const IconsMap& icons_map) {
  for (const auto& url_icon : icons_map) {
    for (const SkBitmap& icon : url_icon.second) {
      if (!icon.empty() && icon.width() == icon.height()) {
        for (const auto& info : icon_infos) {
          if (info.url == url_icon.first) {
            square_icons->push_back(icon);
          }
        }
      }
    }
  }
}

// Append non-empty square icons from |bitmaps| onto the |square_icons| list.
void AddSquareIconsFromBitmaps(
    std::vector<SkBitmap>* square_icons,
    const std::map<SquareSizePx, SkBitmap>& bitmaps) {
  for (const std::pair<const SquareSizePx, SkBitmap>& icon : bitmaps) {
    DCHECK_EQ(icon.first, icon.second.width());
    DCHECK_EQ(icon.first, icon.second.height());
    if (!icon.second.empty())
      square_icons->push_back(icon.second);
  }
}

// Populate |web_app_info|'s shortcuts_menu_item_infos vector using the
// blink::Manifest's shortcuts vector.
std::vector<WebApplicationShortcutsMenuItemInfo>
UpdateShortcutsMenuItemInfosFromManifest(
    const std::vector<blink::Manifest::ShortcutItem>& shortcuts) {
  std::vector<WebApplicationShortcutsMenuItemInfo> web_app_shortcut_infos;
  int num_shortcut_icons = 0;
  for (const auto& shortcut : shortcuts) {
    WebApplicationShortcutsMenuItemInfo shortcut_info;
    shortcut_info.name = shortcut.name;
    shortcut_info.url = shortcut.url;

    std::vector<WebApplicationShortcutsMenuItemInfo::Icon> shortcut_icons;
    for (const auto& icon : shortcut.icons) {
      WebApplicationShortcutsMenuItemInfo::Icon info;

      // Filter out non-square or too large icons.
      auto valid_size_it = std::find_if(
          icon.sizes.begin(), icon.sizes.end(), [](const gfx::Size& size) {
            return size.width() == size.height() &&
                   size.width() <= kMaxIconSize;
          });
      if (valid_size_it == icon.sizes.end())
        continue;
      // TODO(https://crbug.com/1071308): Take the declared icon density and
      // sizes into account.
      info.square_size_px = valid_size_it->width();

      DCHECK_LE(num_shortcut_icons, kMaxIcons);
      if (num_shortcut_icons < kMaxIcons) {
        info.url = icon.src;
        shortcut_icons.push_back(std::move(info));
        ++num_shortcut_icons;
      }
      if (num_shortcut_icons == kMaxIcons)
        break;
    }

    // If any icons are specified in the manifest, they take precedence over
    // any we picked up from web_app_info.
    if (!shortcut_icons.empty())
      shortcut_info.shortcut_icon_infos = std::move(shortcut_icons);
    web_app_shortcut_infos.push_back(std::move(shortcut_info));
  }

  return web_app_shortcut_infos;
}

apps::ShareTarget::Method ToAppsShareTargetMethod(
    blink::Manifest::ShareTarget::Method method) {
  switch (method) {
    case blink::Manifest::ShareTarget::Method::kGet:
      return apps::ShareTarget::Method::kGet;
    case blink::Manifest::ShareTarget::Method::kPost:
      return apps::ShareTarget::Method::kPost;
  }
  NOTREACHED();
}

apps::ShareTarget::Enctype ToAppsShareTargetEnctype(
    blink::Manifest::ShareTarget::Enctype enctype) {
  switch (enctype) {
    case blink::Manifest::ShareTarget::Enctype::kFormUrlEncoded:
      return apps::ShareTarget::Enctype::kFormUrlEncoded;
    case blink::Manifest::ShareTarget::Enctype::kMultipartFormData:
      return apps::ShareTarget::Enctype::kMultipartFormData;
  }
  NOTREACHED();
}

base::Optional<apps::ShareTarget> ToWebAppShareTarget(
    const base::Optional<blink::Manifest::ShareTarget>& share_target) {
  if (!share_target) {
    return base::nullopt;
  }
  apps::ShareTarget apps_share_target;
  apps_share_target.action = share_target->action;
  apps_share_target.method = ToAppsShareTargetMethod(share_target->method);
  apps_share_target.enctype = ToAppsShareTargetEnctype(share_target->enctype);

  if (share_target->params.title.has_value()) {
    apps_share_target.params.title =
        base::UTF16ToUTF8(*share_target->params.title);
  }
  if (share_target->params.text.has_value()) {
    apps_share_target.params.text =
        base::UTF16ToUTF8(*share_target->params.text);
  }
  if (share_target->params.url.has_value()) {
    apps_share_target.params.url = base::UTF16ToUTF8(*share_target->params.url);
  }

  for (const auto& file_filter : share_target->params.files) {
    apps::ShareTarget::Files apps_share_target_files;
    apps_share_target_files.name = base::UTF16ToUTF8(file_filter.name);

    for (const auto& file_type : file_filter.accept) {
      apps_share_target_files.accept.push_back(base::UTF16ToUTF8(file_type));
    }

    apps_share_target.params.files.push_back(
        std::move(apps_share_target_files));
  }

  return std::move(apps_share_target);
}

}  // namespace

void UpdateWebAppInfoFromManifest(const blink::Manifest& manifest,
                                  WebApplicationInfo* web_app_info) {
  // Give the full length name priority if it's not empty.
  base::string16 name = manifest.name.value_or(base::string16());
  if (!name.empty())
    web_app_info->title = name;
  else if (manifest.short_name)
    web_app_info->title = *manifest.short_name;

  // Set the url based on the manifest value, if any.
  if (manifest.start_url.is_valid())
    web_app_info->start_url = manifest.start_url;

  if (manifest.scope.is_valid())
    web_app_info->scope = manifest.scope;

  if (manifest.theme_color) {
    web_app_info->theme_color =
        SkColorSetA(*manifest.theme_color, SK_AlphaOPAQUE);
  }

  if (manifest.background_color) {
    web_app_info->background_color =
        SkColorSetA(*manifest.background_color, SK_AlphaOPAQUE);
  }

  if (manifest.display != DisplayMode::kUndefined)
    web_app_info->display_mode = manifest.display;

  if (!manifest.display_override.empty())
    web_app_info->display_override = manifest.display_override;

  // Create the WebApplicationInfo icons list *outside* of |web_app_info|, so
  // that we can decide later whether or not to replace the existing icons array
  // (conditionally on whether there were any that didn't have purpose ANY).
  std::vector<WebApplicationIconInfo> web_app_icons;
  bool has_purpose_any = false;
  for (const auto& icon : manifest.icons) {
    // An icon's purpose vector should never be empty (the manifest parser
    // should have added ANY if there was no purpose specified in the manifest).
    DCHECK(!icon.purpose.empty());

    for (IconPurpose purpose : icon.purpose) {
      if (purpose != IconPurpose::ANY && purpose != IconPurpose::MASKABLE)
        continue;

      WebApplicationIconInfo info;

      if (!icon.sizes.empty()) {
        // Filter out non-square or too large icons.
        auto valid_size = std::find_if(icon.sizes.begin(), icon.sizes.end(),
                                       [](const gfx::Size& size) {
                                         return size.width() == size.height() &&
                                                size.width() <= kMaxIconSize;
                                       });
        if (valid_size == icon.sizes.end())
          continue;
        // TODO(https://crbug.com/1071308): Take the declared icon density and
        // sizes into account.
        info.square_size_px = valid_size->width();
      }

      info.url = icon.src;
      info.purpose = purpose;
      web_app_icons.push_back(std::move(info));

      if (purpose == IconPurpose::ANY)
        has_purpose_any = true;

      // Limit the number of icons we store on the user's machine.
      if (web_app_icons.size() == kMaxIcons)
        break;
    }
    // Limit the number of icons we store on the user's machine.
    if (web_app_icons.size() == kMaxIcons)
      break;
  }
  // If any icons are specified in the manifest, they take precedence over any
  // we picked up from the web_app stuff.
  if (has_purpose_any)
    web_app_info->icon_infos = std::move(web_app_icons);

  web_app_info->file_handlers = manifest.file_handlers;

  web_app_info->share_target = ToWebAppShareTarget(manifest.share_target);

  web_app_info->protocol_handlers = manifest.protocol_handlers;

  // If any shortcuts are specified in the manifest, they take precedence over
  // any we picked up from the web_app stuff.
  if (!manifest.shortcuts.empty() &&
      base::FeatureList::IsEnabled(
          features::kDesktopPWAsAppIconShortcutsMenu)) {
    web_app_info->shortcuts_menu_item_infos =
        UpdateShortcutsMenuItemInfosFromManifest(manifest.shortcuts);
  }
}

std::vector<GURL> GetValidIconUrlsToDownload(
    const WebApplicationInfo& web_app_info) {
  std::vector<GURL> web_app_info_icon_urls;
  for (const WebApplicationIconInfo& info : web_app_info.icon_infos) {
    if (!info.url.is_valid())
      continue;
    web_app_info_icon_urls.push_back(info.url);
  }
  if (base::FeatureList::IsEnabled(
          features::kDesktopPWAsAppIconShortcutsMenu)) {
    // Also add shortcut icon urls, so they can be downloaded.
    for (const auto& shortcut : web_app_info.shortcuts_menu_item_infos) {
      for (const auto& icon : shortcut.shortcut_icon_infos) {
        if (!icon.url.is_valid())
          continue;
        web_app_info_icon_urls.push_back(icon.url);
      }
    }
  }
  return web_app_info_icon_urls;
}

void PopulateShortcutItemIcons(WebApplicationInfo* web_app_info,
                               const IconsMap* icons_map) {
  for (auto& shortcut : web_app_info->shortcuts_menu_item_infos) {
    SizeToBitmap shortcut_icon_bitmaps;
    for (const auto& icon : shortcut.shortcut_icon_infos) {
      auto it = icons_map->find(icon.url);
      if (it != icons_map->end()) {
        std::set<SquareSizePx> sizes_to_generate;
        sizes_to_generate.emplace(icon.square_size_px);
        SizeToBitmap resized_bitmaps(
            ConstrainBitmapsToSizes(it->second, sizes_to_generate));

        // Don't overwrite as a shortcut item could have multiple icon urls.
        shortcut_icon_bitmaps.insert(resized_bitmaps.begin(),
                                     resized_bitmaps.end());
      }
    }
    web_app_info->shortcuts_menu_icons_bitmaps.emplace_back(
        shortcut_icon_bitmaps);
  }
}

void FilterAndResizeIconsGenerateMissing(WebApplicationInfo* web_app_info,
                                         const IconsMap* icons_map) {
  if (base::FeatureList::IsEnabled(
          features::kDesktopPWAsAppIconShortcutsMenu) &&
      icons_map) {
    PopulateShortcutItemIcons(web_app_info, icons_map);
  }

  std::vector<WebApplicationIconInfo> icon_infos_any;
  std::vector<WebApplicationIconInfo> icon_infos_maskable;
  for (WebApplicationIconInfo& icon_info : web_app_info->icon_infos) {
    switch (icon_info.purpose) {
      case IconPurpose::ANY:
        icon_infos_any.push_back(icon_info);
        break;
      case IconPurpose::MASKABLE:
        icon_infos_maskable.push_back(icon_info);
        break;
      case IconPurpose::MONOCHROME:
        // Not used.
        break;
    }
  }

  std::vector<SkBitmap> square_icons_any;
  std::vector<SkBitmap> square_icons_maskable;
  if (icons_map) {
    AddSquareIconsFromMapMatchingIconInfos(&square_icons_any, icon_infos_any,
                                           *icons_map);
    AddSquareIconsFromMapMatchingIconInfos(&square_icons_maskable,
                                           icon_infos_maskable, *icons_map);
    // Fall back to using all icons from |icons_map| if none match icon_infos.
    if (square_icons_any.empty())
      AddSquareIconsFromMap(&square_icons_any, *icons_map);
  }
  AddSquareIconsFromBitmaps(&square_icons_any, web_app_info->icon_bitmaps_any);

  for (SkBitmap& bitmap : square_icons_maskable) {
    // Retain any bitmaps provided as input to the installation.
    if (web_app_info->icon_bitmaps_maskable.count(bitmap.width()) == 0)
      web_app_info->icon_bitmaps_maskable[bitmap.width()] = std::move(bitmap);
  }

  base::char16 icon_letter =
      web_app_info->title.empty()
          ? GenerateIconLetterFromUrl(web_app_info->start_url)
          : GenerateIconLetterFromAppName(web_app_info->title);
  web_app_info->generated_icon_color = SK_ColorTRANSPARENT;
  // Ensure that all top-level icons that are in web_app_info with  Purpose::ANY
  // are present, by generating icons for any sizes that have failed to
  // download. This ensures that the created manifest for the web app does not
  // contain links to icons that are not actually created and linked on disk.
  // TODO(https://crbug.com/1029223): Don't resize before writing to disk, it's
  // not necessary and would simplify this code path to remove.
  SizeToBitmap size_to_icons = ResizeIconsAndGenerateMissing(
      square_icons_any, SizesToGenerate(), icon_letter,
      &web_app_info->generated_icon_color, &web_app_info->is_generated_icon);

  for (auto& item : size_to_icons) {
    // Retain any bitmaps provided as input to the installation.
    if (web_app_info->icon_bitmaps_any.count(item.first) == 0)
      web_app_info->icon_bitmaps_any[item.first] = std::move(item.second);
  }
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
