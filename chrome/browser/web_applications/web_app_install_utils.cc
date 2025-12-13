// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_utils.h"

#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/shortcuts/shortcut_icon_generator.h"
#include "chrome/browser/ssl/chrome_security_state_tab_helper.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_manager.h"
#include "chrome/browser/web_applications/policy/pre_redirection_url_observer.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_chromeos_data.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "chrome/browser/web_applications/web_app_proto_utils.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/icon_info.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/webapps/browser/banners/app_banner_settings_helper.h"
#include "components/webapps/browser/installable/installable_evaluator.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_declaration.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/experiences/system_web_apps/types/system_web_app_data.h"
#endif

namespace web_app {

class WebAppRegistrar;

namespace {

// We restrict the number of icons to limit disk usage per installed PWA. This
// value can change overtime as new features are added.
constexpr int kMaxIcons = 20;
constexpr SquareSizePx kMaxIconSize =
    webapps::InstallableEvaluator::kMaximumIconSizeInPx;

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
// if they are also in |manifest_icons|.
void AddSquareIconsFromMapMatchingIconInfos(
    std::vector<SkBitmap>* square_icons,
    const std::vector<apps::IconInfo>& manifest_icons,
    const IconsMap& icons_map) {
  for (const auto& url_icon : icons_map) {
    for (const SkBitmap& icon : url_icon.second) {
      if (!icon.empty() && icon.width() == icon.height()) {
        for (const auto& info : manifest_icons) {
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

std::vector<SquareSizePx> GetSquareSizePxs(
    const std::map<SquareSizePx, SkBitmap>& icon_bitmaps) {
  std::vector<SquareSizePx> sizes;
  sizes.reserve(icon_bitmaps.size());
  for (const std::pair<const SquareSizePx, SkBitmap>& item : icon_bitmaps)
    sizes.push_back(item.first);
  return sizes;
}

std::vector<SquareSizePx> GetSquareSizePxs(const IconBitmaps& icon_bitmaps,
                                           IconPurpose purpose) {
  switch (purpose) {
    case IconPurpose::ANY:
      return GetSquareSizePxs(icon_bitmaps.any);
    case IconPurpose::MASKABLE:
      return GetSquareSizePxs(icon_bitmaps.maskable);
    case IconPurpose::MONOCHROME:
      return GetSquareSizePxs(icon_bitmaps.monochrome);
  }
}

// Returns a new vector of item infos with `downloaded_icon_sizes` set from
// `shortcuts_menu_icon_bitmaps` and other info copied from
// `shortcuts_menu_items`.
std::vector<WebAppShortcutsMenuItemInfo> GetShortcutsMenuInfoWithIconSizes(
    const std::vector<WebAppShortcutsMenuItemInfo>& shortcuts_menu_items,
    const ShortcutsMenuIconBitmaps& shortcuts_menu_icon_bitmaps) {
  // Due to the bitmaps possibly being not populated (see
  // https://crbug.com/1427444), we create empty bitmaps in that case. We
  // continue to check to make sure that there aren't MORE bitmaps than
  // items.
  CHECK_LE(shortcuts_menu_icon_bitmaps.size(), shortcuts_menu_items.size());
  std::vector<WebAppShortcutsMenuItemInfo> items_with_sizes;
  items_with_sizes.reserve(shortcuts_menu_items.size());
  IconBitmaps empty_icon_bitmaps;
  for (size_t i = 0; i < shortcuts_menu_items.size(); ++i) {
    const IconBitmaps* shortcut_icon_bitmaps;
    if (i < shortcuts_menu_icon_bitmaps.size()) {
      shortcut_icon_bitmaps = &shortcuts_menu_icon_bitmaps[i];
    } else {
      shortcut_icon_bitmaps = &empty_icon_bitmaps;
    }

    WebAppShortcutsMenuItemInfo item_info = shortcuts_menu_items[i];
    for (IconPurpose purpose : kIconPurposes) {
      item_info.downloaded_icon_sizes.SetSizesForPurpose(
          purpose, GetSquareSizePxs(*shortcut_icon_bitmaps, purpose));
    }

    items_with_sizes.push_back(std::move(item_info));
  }
  return items_with_sizes;
}

void PopulateShortcutItemIcons(WebAppInstallInfo* web_app_info,
                               const IconsMap& icons_map) {
  web_app_info->shortcuts_menu_icon_bitmaps.clear();
  for (auto& shortcut : web_app_info->shortcuts_menu_item_infos) {
    IconBitmaps shortcut_icon_bitmaps;

    for (IconPurpose purpose : kIconPurposes) {
      std::map<SquareSizePx, SkBitmap> bitmaps;
      for (const auto& icon :
           shortcut.GetShortcutIconInfosForPurpose(purpose)) {
        auto it = icons_map.find(icon.url);
        // Resize bitmaps only if the icon size is non-zero. The alternative
        // happens mostly for icons with a size of ANY passed in via the
        // manifest, for which we do not need to resize since it is downloaded
        // in all sizes..
        if (it != icons_map.end()) {
          if (icon.square_size_px != 0) {
            std::set<SquareSizePx> sizes_to_generate;
            sizes_to_generate.emplace(icon.square_size_px);
            SizeToBitmap resized_bitmaps(
                ConstrainBitmapsToSizes(it->second, sizes_to_generate));

            // Don't overwrite as a shortcut item could have multiple icon urls.
            bitmaps.insert(resized_bitmaps.begin(), resized_bitmaps.end());
          } else {
            // For icons that do not need resizing, pass in the bitmaps as it
            // is, barring ones that have no intrinsic size.
            for (const auto& bitmap : it->second) {
              if (!bitmap.empty()) {
                bitmaps[bitmap.width()] = bitmap;
              }
            }
          }
        }
      }
      shortcut_icon_bitmaps.SetBitmapsForPurpose(purpose, std::move(bitmaps));
    }

    web_app_info->shortcuts_menu_icon_bitmaps.emplace_back(
        std::move(shortcut_icon_bitmaps));
  }
  CHECK_EQ(web_app_info->shortcuts_menu_icon_bitmaps.size(),
           web_app_info->shortcuts_menu_item_infos.size());
}

// Reconcile the file handling icons that were specified in the manifest with
// the icons we were successfully able to download. Store the actual bitmaps and
// update the icon metadata in `web_app_info`.
void PopulateFileHandlingIcons(WebAppInstallInfo* web_app_info,
                               const IconsMap& icons_map,
                               IconsMap& other_icon_bitmaps) {
  // Before starting, each `apps::IconInfo` in `web_app_info` has a source URL
  // and a purpose, but no size. Replace with structs that copy the URL and
  // purpose and set the size based on what is found in `icons_map`.
  for (auto& file_handler : web_app_info->file_handlers) {
    if (!WebAppFileHandlerManager::IconsEnabled()) {
      DCHECK(file_handler.downloaded_icons.empty());
      continue;
    }

    std::vector<apps::IconInfo> manifest_icons;

    for (const auto& icon_info_without_size : file_handler.downloaded_icons) {
      const GURL& src = icon_info_without_size.url;
      // Only store bitmaps for this URL if it's the first time we've seen it.
      bool bitmaps_already_saved_for_url =
          other_icon_bitmaps.find(src) != other_icon_bitmaps.end();
      const auto& downloaded_bitmaps_for_url = icons_map.find(src);
      if (downloaded_bitmaps_for_url == icons_map.end())
        continue;

      for (const SkBitmap& bitmap : downloaded_bitmaps_for_url->second) {
        // Filter out bitmaps that are empty, non-square or are too large.
        if (bitmap.empty() || bitmap.width() != bitmap.height() ||
            bitmap.width() > kMaxIconSize) {
          continue;
        }

        // Add the size to the FileHandler icon metadata.
        apps::IconInfo icon_info_with_size(icon_info_without_size);
        icon_info_with_size.square_size_px = bitmap.width();
        manifest_icons.push_back(std::move(icon_info_with_size));

        // Add the bitmap to `other_icon_bitmaps`.
        if (!bitmaps_already_saved_for_url)
          other_icon_bitmaps[src].push_back(bitmap);
      }
    }
    file_handler.downloaded_icons = std::move(manifest_icons);
  }
}

// Reconcile the home tab icons that were specified in the manifest with
// the icons we were successfully able to download. Store the actual bitmaps and
// update the icon metadata in `web_app_info`.
void PopulateHomeTabIcons(WebAppInstallInfo* web_app_info,
                          const IconsMap& icons_map,
                          IconsMap& other_icon_bitmaps) {
  if (!HomeTabIconsExistInTabStrip(*web_app_info)) {
    return;
  }

  const auto& home_tab = std::get<blink::Manifest::HomeTabParams>(
      web_app_info->tab_strip.value().home_tab);

  for (const auto& icon : home_tab.icons) {
    // An icon's purpose vector should never be empty (the manifest parser
    // should have added ANY if there was no purpose specified in the manifest).
    DCHECK(!icon.purpose.empty());

    // Only store bitmaps for this URL if this is the first time we've seen it.
    bool bitmaps_already_saved_for_url =
        other_icon_bitmaps.find(icon.src) != other_icon_bitmaps.end();
    const auto& downloaded_bitmaps_for_url = icons_map.find(icon.src);

    if (downloaded_bitmaps_for_url == icons_map.end()) {
      continue;
    }

    for (const SkBitmap& bitmap : downloaded_bitmaps_for_url->second) {
      // Filter out bitmaps that are empty, non-square or are too large.
      if (bitmap.empty() || bitmap.width() != bitmap.height() ||
          bitmap.width() > kMaxIconSize) {
        continue;
      }

      // Add the bitmap to 'other_icon_bitmaps'.
      if (!bitmaps_already_saved_for_url) {
        other_icon_bitmaps[icon.src].emplace_back(bitmap);
      }
    }

    DCHECK(other_icon_bitmaps.size() <= kMaxIcons);
  }
}

apps::FileHandler::LaunchType ToFileHandlerLaunchType(
    blink::mojom::ManifestFileHandler::LaunchType launch_type) {
  switch (launch_type) {
    case blink::mojom::ManifestFileHandler::LaunchType::kSingleClient:
      return apps::FileHandler::LaunchType::kSingleClient;
    case blink::mojom::ManifestFileHandler::LaunchType::kMultipleClients:
      return apps::FileHandler::LaunchType::kMultipleClients;
  }
}

void PopulateTrustedIconsFromDownloadedBitmapsAndMetadata(
    const IconsMap& icons_downloaded,
    const std::vector<apps::IconInfo>& icon_metadata,
    std::map<SquareSizePx, SkBitmap>& output_size_to_bitmaps) {
  CHECK(output_size_to_bitmaps.empty());
  std::vector<SkBitmap> square_icons_matching_infos;
  // First, choose all bitmaps from `icons_downloaded` that share the same url
  // as the entries in `icon_metadata` in `square_icons_matching_infos`.
  AddSquareIconsFromMapMatchingIconInfos(&square_icons_matching_infos,
                                         icon_metadata, icons_downloaded);

  // Second, start populating the `output_size_to_bitmaps` map with all the
  // parsed bitmaps, once per size.
  for (auto& icon : square_icons_matching_infos) {
    output_size_to_bitmaps[icon.width()] = icon;
  }

  // Third, resize existing icons if any and populate `output_size_to_bitmaps`
  // with the bitmaps whose sizes are not populated previously.
  SizeToBitmap sizes_to_icons = ConstrainBitmapsToSizes(
      square_icons_matching_infos, web_app::SizesToGenerate());
  for (auto& [size, icon] : sizes_to_icons) {
    if (!base::Contains(output_size_to_bitmaps, size)) {
      output_size_to_bitmaps[size] = std::move(icon);
    }
  }
}

}  // namespace

void PopulateFileHandlerInfoFromManifest(
    const std::vector<blink::mojom::ManifestFileHandlerPtr>&
        manifest_file_handlers,
    const GURL& app_scope,
    WebAppInstallInfo* web_app_info) {
  apps::FileHandlers web_app_file_handlers;

  for (const auto& manifest_file_handler : manifest_file_handlers) {
    DCHECK(manifest_file_handler);
    apps::FileHandler web_app_file_handler;
    web_app_file_handler.action = manifest_file_handler->action;
    web_app_file_handler.display_name = manifest_file_handler->name;
    web_app_file_handler.launch_type =
        ToFileHandlerLaunchType(manifest_file_handler->launch_type);

    for (const auto& it : manifest_file_handler->accept) {
      apps::FileHandler::AcceptEntry web_app_accept_entry;
      web_app_accept_entry.mime_type = base::UTF16ToUTF8(it.first);
      for (const auto& manifest_file_extension : it.second) {
        web_app_accept_entry.file_extensions.insert(
            base::UTF16ToUTF8(manifest_file_extension));
      }
      web_app_file_handler.accept.push_back(std::move(web_app_accept_entry));
    }

    if (WebAppFileHandlerManager::IconsEnabled()) {
      for (const auto& image_resource : manifest_file_handler->icons) {
        for (const auto manifest_purpose : image_resource.purpose) {
          apps::IconInfo icon_info;
          icon_info.url = image_resource.src;
          icon_info.purpose =
              ManifestPurposeToIconInfoPurpose(manifest_purpose);
          if (base::Contains(image_resource.sizes, gfx::Size()) &&
              image_resource.src.spec().find(".svg") != std::string::npos) {
            web_app_info->icons_with_size_any
                .file_handling_icons[manifest_purpose] = image_resource.src;
          }

          web_app_file_handler.downloaded_icons.push_back(std::move(icon_info));
          // The list will be pruned and the sizes will be filled in when images
          // are actually downloaded.
        }

        // Keep track of the sizes passed in via the manifest which will be
        // later used to compute how many SVG icons of size:any we need to
        // download.
        if (!web_app_info->icons_with_size_any.file_handling_icons.empty()) {
          for (const auto& icon_size : image_resource.sizes) {
            if (icon_size == gfx::Size()) {
              continue;
            }
            web_app_info->icons_with_size_any.file_handling_icon_provided_sizes
                .emplace(icon_size);
          }
        }
      }
    }

    web_app_file_handlers.push_back(std::move(web_app_file_handler));
  }

  web_app_info->file_handlers = std::move(web_app_file_handlers);
}

void PopulateOtherIcons(WebAppInstallInfo* web_app_info,
                        const IconsMap& icons_map) {
  IconsMap& other_icon_bitmaps = web_app_info->other_icon_bitmaps;
  other_icon_bitmaps.clear();
  PopulateShortcutItemIcons(web_app_info, icons_map);
  PopulateFileHandlingIcons(web_app_info, icons_map, other_icon_bitmaps);
  PopulateHomeTabIcons(web_app_info, icons_map, other_icon_bitmaps);
}

void PopulateProductIcons(WebAppInstallInfo* web_app_info,
                          const IconsMap* icons_map) {
  std::vector<apps::IconInfo> manifest_icons_any;
  std::vector<apps::IconInfo> manifest_icons_maskable;
  std::vector<apps::IconInfo> manifest_icons_monochrome;
  for (apps::IconInfo& icon_info : web_app_info->manifest_icons) {
    switch (icon_info.purpose) {
      case apps::IconInfo::Purpose::kAny:
        manifest_icons_any.push_back(icon_info);
        break;
      case apps::IconInfo::Purpose::kMaskable:
        manifest_icons_maskable.push_back(icon_info);
        break;
      case apps::IconInfo::Purpose::kMonochrome:
        manifest_icons_monochrome.push_back(icon_info);
        break;
    }
  }
  std::vector<SkBitmap> square_icons_any;
  std::vector<SkBitmap> square_icons_maskable;
  std::vector<SkBitmap> square_icons_monochrome;
  if (icons_map) {
    AddSquareIconsFromMapMatchingIconInfos(&square_icons_any,
                                           manifest_icons_any, *icons_map);
    AddSquareIconsFromMapMatchingIconInfos(&square_icons_maskable,
                                           manifest_icons_maskable, *icons_map);
    AddSquareIconsFromMapMatchingIconInfos(
        &square_icons_monochrome, manifest_icons_monochrome, *icons_map);
    // Fall back to using all icons from |icons_map| if none match
    // manifest_icons.
    if (square_icons_any.empty())
      AddSquareIconsFromMap(&square_icons_any, *icons_map);
  }
  AddSquareIconsFromBitmaps(&square_icons_any, web_app_info->icon_bitmaps.any);

  // Retain any bitmaps provided as input to the installation.
  for (auto& icon : square_icons_maskable) {
    if (!base::Contains(web_app_info->icon_bitmaps.maskable, icon.width())) {
      web_app_info->icon_bitmaps.maskable[icon.width()] = std::move(icon);
    }
  }

  for (auto& icon : square_icons_monochrome) {
    if (!base::Contains(web_app_info->icon_bitmaps.monochrome, icon.width())) {
      web_app_info->icon_bitmaps.monochrome[icon.width()] = std::move(icon);
    }
  }

  std::u16string icon_letter =
      web_app_info->title.empty()
          ? shortcuts::GenerateIconLetterFromUrl(web_app_info->start_url())
          : shortcuts::GenerateIconLetterFromName(web_app_info->title.value());

  // Ensure that all top-level icons that are in web_app_info with  Purpose::ANY
  // are present, by generating icons for any sizes that have failed to
  // download. This ensures that the created manifest for the web app does not
  // contain links to icons that are not actually created and linked on disk.
  // TODO(crbug.com/40661228): Don't resize before writing to disk, it's
  // not necessary and would simplify this code path to remove.
  SizeToBitmap size_to_icons = ResizeIconsAndGenerateMissing(
      square_icons_any, SizesToGenerate(), icon_letter,
      &web_app_info->is_generated_icon);

  for (auto& item : size_to_icons) {
    // Retain any bitmaps provided as input to the installation.
    if (web_app_info->icon_bitmaps.any.count(item.first) == 0)
      web_app_info->icon_bitmaps.any[item.first] = std::move(item.second);
  }
}

void PopulateTrustedIconBitmaps(WebAppInstallInfo& web_app_info,
                                const IconsMap& icons_map) {
  // Exit early if there have been no downloaded icons, as then the whole
  // `trusted_icons` metadata will be empty.
  if (icons_map.empty()) {
    return;
  }

  // Construct the trusted icon metadata per purpose.
  std::vector<apps::IconInfo> trusted_icons_any;
  std::vector<apps::IconInfo> trusted_icons_maskable;
  std::vector<apps::IconInfo> trusted_icons_monochrome;
  for (apps::IconInfo& icon_info : web_app_info.trusted_icons) {
    switch (icon_info.purpose) {
      case apps::IconInfo::Purpose::kAny:
        trusted_icons_any.push_back(icon_info);
        break;
      case apps::IconInfo::Purpose::kMaskable:
        trusted_icons_maskable.push_back(icon_info);
        break;
      case apps::IconInfo::Purpose::kMonochrome:
        trusted_icons_monochrome.push_back(icon_info);
        break;
    }
  }

  PopulateTrustedIconsFromDownloadedBitmapsAndMetadata(
      icons_map, trusted_icons_any, web_app_info.trusted_icon_bitmaps.any);
  PopulateTrustedIconsFromDownloadedBitmapsAndMetadata(
      icons_map, trusted_icons_maskable,
      web_app_info.trusted_icon_bitmaps.maskable);
  PopulateTrustedIconsFromDownloadedBitmapsAndMetadata(
      icons_map, trusted_icons_monochrome,
      web_app_info.trusted_icon_bitmaps.monochrome);
}

void RecordDownloadedIconsResultAndHttpStatusCodes(
    IconsDownloadedResult result,
    const DownloadedIconsHttpResults& icons_http_results) {
  RecordDownloadedIconsHttpResultsCodeClass(
      "WebApp.Icon.HttpStatusCodeClassOnCreate", result, icons_http_results);

  base::UmaHistogramEnumeration("WebApp.Icon.DownloadedResultOnCreate", result);
  RecordDownloadedIconHttpStatusCodes(
      "WebApp.Icon.DownloadedHttpStatusCodeOnCreate", icons_http_results);
}

void RecordDownloadedIconsHttpResultsCodeClass(
    std::string_view histogram_name,
    IconsDownloadedResult result,
    const DownloadedIconsHttpResults& icons_http_results) {
  if (result != IconsDownloadedResult::kCompleted)
    return;

  for (const auto& url_and_http_status_code : icons_http_results) {
    int http_status_code = url_and_http_status_code.second;
    if (http_status_code != 0) {
      DCHECK_LE(100, http_status_code);
      DCHECK_GT(600, http_status_code);
      base::UmaHistogramExactLinear(histogram_name.data(),
                                    http_status_code / 100, 5);
    }
  }
}

void RecordDownloadedIconHttpStatusCodes(
    std::string_view histogram_name,
    const DownloadedIconsHttpResults& icons_http_results) {
  if (icons_http_results.empty())
    return;

  // Do not use UMA_HISTOGRAM_... macros here, as it caches the Histogram
  // instance and thus only works if |histogram_name| is constant.
  base::HistogramBase* counter = base::CustomHistogram::FactoryGet(
      histogram_name.data(), net::HttpUtil::GetStatusCodesForHistogram(),
      base::HistogramBase::kUmaTargetedHistogramFlag);

  // A web app may contain arbitrary number of icons. The histogram assumes that
  // most of them fail with same http status codes and counts each http status
  // code only once.
  std::vector<int> http_status_codes;
  http_status_codes.reserve(icons_http_results.size());
  for (const auto& url_and_http_status_code : icons_http_results)
    http_status_codes.push_back(url_and_http_status_code.second);

  base::flat_set<int> unique_http_status_codes{std::move(http_status_codes)};

  for (int http_status_code : unique_http_status_codes)
    counter->Add(net::HttpUtil::MapStatusCodeForHistogram(http_status_code));
}

WebAppManagement::Type ConvertExternalInstallSourceToSource(
    ExternalInstallSource external_install_source) {
  return ConvertInstallSurfaceToWebAppSource(
      ConvertExternalInstallSourceToInstallSource(external_install_source));
}

webapps::WebappInstallSource ConvertExternalInstallSourceToInstallSource(
    ExternalInstallSource external_install_source) {
  switch (external_install_source) {
    case ExternalInstallSource::kInternalDefault:
      return webapps::WebappInstallSource::INTERNAL_DEFAULT;
    case ExternalInstallSource::kExternalDefault:
      return webapps::WebappInstallSource::EXTERNAL_DEFAULT;
    case ExternalInstallSource::kExternalPolicy:
      return webapps::WebappInstallSource::EXTERNAL_POLICY;
    case ExternalInstallSource::kSystemInstalled:
      return webapps::WebappInstallSource::SYSTEM_DEFAULT;
    case ExternalInstallSource::kKiosk:
      return webapps::WebappInstallSource::KIOSK;
    case ExternalInstallSource::kExternalLockScreen:
      return webapps::WebappInstallSource::EXTERNAL_LOCK_SCREEN;
    case ExternalInstallSource::kInternalMicrosoft365Setup:
      return webapps::WebappInstallSource::MICROSOFT_365_SETUP;
  }
}

webapps::WebappUninstallSource ConvertExternalInstallSourceToUninstallSource(
    ExternalInstallSource external_install_source) {
  switch (external_install_source) {
    case ExternalInstallSource::kInternalDefault:
      return webapps::WebappUninstallSource::kInternalPreinstalled;
    case ExternalInstallSource::kExternalDefault:
      return webapps::WebappUninstallSource::kExternalPreinstalled;
    case ExternalInstallSource::kExternalPolicy:
      return webapps::WebappUninstallSource::kExternalPolicy;
    case ExternalInstallSource::kSystemInstalled:
      return webapps::WebappUninstallSource::kSystemPreinstalled;
    case ExternalInstallSource::kKiosk:
      NOTREACHED() << "Kiosk apps should not be uninstalled";
    case ExternalInstallSource::kExternalLockScreen:
      return webapps::WebappUninstallSource::kExternalLockScreen;
    case ExternalInstallSource::kInternalMicrosoft365Setup:
      NOTREACHED() << "Microsoft 365 apps should not be uninstalled externally";
  }
}

WebAppManagement::Type ConvertInstallSurfaceToWebAppSource(
    webapps::WebappInstallSource install_source) {
  switch (install_source) {
    case webapps::WebappInstallSource::SYNC:
      return WebAppManagement::kSync;

    case webapps::WebappInstallSource::MENU_BROWSER_TAB:
    case webapps::WebappInstallSource::MENU_CUSTOM_TAB:
    case webapps::WebappInstallSource::AUTOMATIC_PROMPT_BROWSER_TAB:
    case webapps::WebappInstallSource::AUTOMATIC_PROMPT_CUSTOM_TAB:
    case webapps::WebappInstallSource::API_BROWSER_TAB:
    case webapps::WebappInstallSource::API_CUSTOM_TAB:
    case webapps::WebappInstallSource::DEVTOOLS:
    case webapps::WebappInstallSource::MANAGEMENT_API:
    case webapps::WebappInstallSource::AMBIENT_BADGE_BROWSER_TAB:
    case webapps::WebappInstallSource::AMBIENT_BADGE_CUSTOM_TAB:
    case webapps::WebappInstallSource::RICH_INSTALL_UI_WEBLAYER:
    case webapps::WebappInstallSource::ML_PROMOTION:
    case webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON:
    case webapps::WebappInstallSource::MENU_CREATE_SHORTCUT:
    case webapps::WebappInstallSource::CHROME_SERVICE:
    case webapps::WebappInstallSource::PROFILE_MENU:
    case webapps::WebappInstallSource::ALMANAC_INSTALL_APP_URI:
    case webapps::WebappInstallSource::WEBAPK_RESTORE:
    case webapps::WebappInstallSource::OOBE_APP_RECOMMENDATIONS:
    case webapps::WebappInstallSource::WEB_INSTALL:
    case webapps::WebappInstallSource::CHROMEOS_HELP_APP:
      return WebAppManagement::kUserInstalled;

    case webapps::WebappInstallSource::IWA_GRAPHICAL_INSTALLER:
    case webapps::WebappInstallSource::IWA_DEV_UI:
    case webapps::WebappInstallSource::IWA_DEV_COMMAND_LINE:
      return WebAppManagement::kIwaUserInstalled;

    case webapps::WebappInstallSource::INTERNAL_DEFAULT:
    case webapps::WebappInstallSource::EXTERNAL_DEFAULT:
      return WebAppManagement::kDefault;

    case webapps::WebappInstallSource::PRELOADED_DEFAULT:
      return WebAppManagement::kApsDefault;

    case webapps::WebappInstallSource::PRELOADED_OEM:
      return WebAppManagement::kOem;

    case webapps::WebappInstallSource::IWA_SHIMLESS_RMA:
      return WebAppManagement::kIwaShimlessRma;

    case webapps::WebappInstallSource::EXTERNAL_POLICY:
      return WebAppManagement::kPolicy;

    case webapps::WebappInstallSource::IWA_EXTERNAL_POLICY:
      return WebAppManagement::kIwaPolicy;

    case webapps::WebappInstallSource::KIOSK:
      return WebAppManagement::kKiosk;

    case webapps::WebappInstallSource::EXTERNAL_LOCK_SCREEN:
    case webapps::WebappInstallSource::SYSTEM_DEFAULT:
      return WebAppManagement::kSystem;

    case webapps::WebappInstallSource::ARC:
      return WebAppManagement::kWebAppStore;

    case webapps::WebappInstallSource::SUB_APP:
      return WebAppManagement::kSubApp;

    case webapps::WebappInstallSource::MICROSOFT_365_SETUP:
      return WebAppManagement::kOneDriveIntegration;
  }
}

void CreateWebAppInstallTabHelpers(content::WebContents* web_contents) {
  webapps::InstallableManager::CreateForWebContents(web_contents);
  ChromeSecurityStateTabHelper::CreateForWebContents(web_contents);
  favicon::CreateContentFaviconDriverForWebContents(web_contents);
  webapps::PreRedirectionURLObserver::CreateForWebContents(web_contents);
}

void SetWebAppManifestFields(const WebAppInstallInfo& web_app_info,
                             WebApp& web_app,
                             bool skip_icons_on_download_failure) {
  // TODO(crbug.com/344718166): ManifestId should already be set the same,
  // otherwise setting it here would be changing the app's ID. This should be a
  // CHECK_EQ instead of a set.
  web_app.SetManifestId(web_app_info.manifest_id());

  DCHECK(!web_app_info.title.empty());
  web_app.SetName(base::UTF16ToUTF8(web_app_info.title.value()));

  web_app.SetStartUrl(web_app_info.start_url());

  web_app.SetDisplayMode(web_app_info.display_mode);
  web_app.SetDisplayModeOverride(web_app_info.display_override);

  web_app.SetBorderlessUrlPatterns(web_app_info.borderless_url_patterns);

  web_app.SetDescription(base::UTF16ToUTF8(web_app_info.description));
  web_app.SetLaunchQueryParams(web_app_info.launch_query_params);
  if (web_app_info.scope.is_valid()) {
    web_app.SetScope(web_app_info.scope);
  } else {
    web_app.SetScope(web_app_info.start_url().GetWithoutFilename());
  }
  CHECK(!web_app.scope().is_empty());

  DCHECK(!web_app_info.theme_color.has_value() ||
         SkColorGetA(*web_app_info.theme_color) == SK_AlphaOPAQUE);
  web_app.SetThemeColor(web_app_info.theme_color);

  DCHECK(!web_app_info.dark_mode_theme_color.has_value() ||
         SkColorGetA(*web_app_info.dark_mode_theme_color) == SK_AlphaOPAQUE);
  web_app.SetDarkModeThemeColor(web_app_info.dark_mode_theme_color);

  DCHECK(!web_app_info.background_color.has_value() ||
         SkColorGetA(*web_app_info.background_color) == SK_AlphaOPAQUE);
  web_app.SetBackgroundColor(web_app_info.background_color);

  DCHECK(!web_app_info.dark_mode_background_color.has_value() ||
         SkColorGetA(*web_app_info.dark_mode_background_color) ==
             SK_AlphaOPAQUE);
  web_app.SetDarkModeBackgroundColor(web_app_info.dark_mode_background_color);

  sync_pb::WebAppSpecifics sync_proto = web_app.sync_proto();
  // Sync proto has already been initialized by setting the start_url and/or
  // manifest_id above.
  CHECK(sync_proto.has_start_url());
  CHECK(sync_proto.has_relative_manifest_id());
  sync_proto.set_name(base::UTF16ToUTF8(web_app_info.title.value()));
  sync_proto.clear_theme_color();
  if (web_app_info.theme_color.has_value()) {
    sync_proto.set_theme_color(web_app_info.theme_color.value());
  }
  sync_proto.clear_scope();
  if (web_app_info.scope.is_valid()) {
    sync_proto.set_scope(web_app_info.scope.spec());
  }
  sync_proto.clear_icon_infos();
  for (const apps::IconInfo& icon_info : web_app_info.manifest_icons) {
    *(sync_proto.add_icon_infos()) = AppIconInfoToSyncProto(icon_info);
  }
  sync_proto.clear_trusted_icons();
  for (const apps::IconInfo& trusted_icon : web_app_info.trusted_icons) {
    *(sync_proto.add_trusted_icons()) = AppIconInfoToSyncProto(trusted_icon);
  }
  web_app.SetSyncProto(std::move(sync_proto));

  if (!skip_icons_on_download_failure) {
    SetWebAppProductIconFields(web_app_info, web_app);
    web_app.SetShortcutsMenuInfo(GetShortcutsMenuInfoWithIconSizes(
        web_app_info.shortcuts_menu_item_infos,
        web_app_info.shortcuts_menu_icon_bitmaps));
  }

  web_app.SetPermissionsPolicy(web_app_info.permissions_policy);

  if (web_app.file_handler_approval_state() == ApiApprovalState::kAllowed &&
      !AreNewFileHandlersASubsetOfOld(web_app.file_handlers(),
                                      web_app_info.file_handlers)) {
    web_app.SetFileHandlerApprovalState(ApiApprovalState::kRequiresPrompt);
  }
  web_app.SetFileHandlers(web_app_info.file_handlers);
  web_app.SetShareTarget(web_app_info.share_target);
  web_app.SetProtocolHandlers(web_app_info.protocol_handlers);
  web_app.SetScopeExtensions(web_app_info.scope_extensions);

  if (base::FeatureList::IsEnabled(features::kWebLockScreenApi))
    web_app.SetLockScreenStartUrl(web_app_info.lock_screen_start_url);

  web_app.SetNoteTakingNewNoteUrl(web_app_info.note_taking_new_note_url);

  web_app.SetManifestUrl(web_app_info.manifest_url);

  web_app.SetLaunchHandler(web_app_info.launch_handler);

  web_app.SetTabStrip(web_app_info.tab_strip);

  if (web_app_info.validated_scope_extensions.has_value()) {
    web_app.SetValidatedScopeExtensions(
        web_app_info.validated_scope_extensions.value());
  }

  web_app.SetIsDiyApp(web_app_info.is_diy_app);

  web_app.SetRelatedApplications(web_app_info.related_applications);
}

void SetWebAppProductIconFields(const WebAppInstallInfo& web_app_info,
                                WebApp& web_app) {
  web_app.SetManifestIcons(web_app_info.manifest_icons);
  web_app.SetIsGeneratedIcon(web_app_info.is_generated_icon);
  web_app.SetTrustedIcons(web_app_info.trusted_icons);

  // Cache size information for icons stored on disk.
  for (IconPurpose purpose : kIconPurposes) {
    web_app.SetDownloadedIconSizes(
        purpose, GetSquareSizePxs(web_app_info.icon_bitmaps, purpose));
    if (web_app_info.trusted_icon_bitmaps.empty() ||
        purpose == IconPurpose::MONOCHROME) {
      continue;
    }
    web_app.SetStoredTrustedIconSizes(
        purpose, GetSquareSizePxs(web_app_info.trusted_icon_bitmaps, purpose));
  }
}

bool CanWebAppUpdateIdentity(const WebApp* web_app) {
  if (web_app->IsPolicyInstalledApp() &&
      base::FeatureList::IsEnabled(
          features::kWebAppManifestPolicyAppIdentityUpdate)) {
    return true;
  }

  // WebAppChromeOsData::oem_installed is not included in this statement as
  // we would like to keep WebAppManagement::kOem and
  // WebAppChromeOsData::oem_installed separate.
  // WebAppChromeOsData::oem_installed will be migrated to
  // WebAppManagement::kOem eventually.
  return web_app->IsPreinstalledApp() || web_app->IsKioskInstalledApp() ||
         web_app->GetSources().HasAny(
             {WebAppManagement::kOem, WebAppManagement::kApsDefault});
}

void ApplyParamsToWebAppInstallInfo(const WebAppInstallParams& install_params,
                                    WebAppInstallInfo& web_app_info) {
  if (install_params.user_display_mode.has_value())
    web_app_info.user_display_mode = install_params.user_display_mode;

  // If `additional_search_terms` was a manifest property, it would be
  // sanitized while parsing the manifest. Since it's not, we sanitize it
  // here.
  for (const std::string& search_term :
       install_params.additional_search_terms) {
    if (!search_term.empty())
      web_app_info.additional_search_terms.push_back(search_term);
  }

  if (install_params.launch_query_params)
    web_app_info.launch_query_params = install_params.launch_query_params;

  if (install_params.install_url.is_valid())
    web_app_info.install_url = install_params.install_url;
}

void ApplyParamsToFinalizeOptions(
    const WebAppInstallParams& install_params,
    WebAppInstallFinalizer::FinalizeOptions& options) {
  if (IsChromeOsDataMandatory()) {
    options.chromeos_data.emplace();
    options.chromeos_data->show_in_launcher =
        install_params.add_to_applications_menu;
    options.chromeos_data->show_in_search_and_shelf =
        install_params.add_to_search;
    options.chromeos_data->show_in_management =
        install_params.add_to_management;
    options.chromeos_data->is_disabled = install_params.is_disabled;
    options.chromeos_data->oem_installed = install_params.oem_installed;
    options.chromeos_data->handles_file_open_intents =
        install_params.handles_file_open_intents;
  }
  options.install_state = install_params.install_state;
  options.add_to_applications_menu = install_params.add_to_applications_menu;
  options.add_to_desktop = install_params.add_to_desktop;
  options.add_to_quick_launch_bar = install_params.add_to_quick_launch_bar;
  options.skip_origin_association_validation =
      install_params.skip_origin_association_validation;
#if BUILDFLAG(IS_CHROMEOS)
  if (install_params.system_app_type.has_value()) {
    options.system_web_app_data.emplace();
    options.system_web_app_data->system_app_type =
        install_params.system_app_type.value();
  }
#endif
}

bool HomeTabIconsExistInTabStrip(const WebAppInstallInfo& web_app_info) {
  if (!web_app_info.tab_strip.has_value()) {
    return false;
  }

  if (!std::holds_alternative<blink::Manifest::HomeTabParams>(
          web_app_info.tab_strip.value().home_tab)) {
    return false;
  }

  const auto& home_tab = std::get<blink::Manifest::HomeTabParams>(
      web_app_info.tab_strip.value().home_tab);

  if (home_tab.icons.empty()) {
    return false;
  }

  return true;
}

bool IsSyncEnabledForApps(Profile* profile) {
  if (!SyncServiceFactory::HasSyncService(profile)) {
    return false;
  }
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
#if BUILDFLAG(IS_CHROMEOS)
  return sync_service->GetUserSettings()->GetSelectedOsTypes().Has(
      syncer::UserSelectableOsType::kOsApps);
#else
  return sync_service->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kApps);
#endif
}

}  // namespace web_app
