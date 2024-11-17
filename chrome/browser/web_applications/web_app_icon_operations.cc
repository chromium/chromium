// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_icon_operations.h"

#include <set>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/extend.h"
#include "base/containers/flat_set.h"
#include "base/values.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "components/services/app_service/public/cpp/icon_info.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace web_app {

namespace {

base::flat_set<GURL> GetAllIconUrlsForSizeAny(
    base::flat_map<IconPurpose, GURL> icon_purpose_to_urls) {
  base::flat_set<GURL> urls;
  for (const auto& data : icon_purpose_to_urls) {
    urls.insert(data.second);
  }
  return urls;
}

void PopulateIconUrlsForSizeAnyIfNeeded(
    std::vector<IconUrlWithSize>& icon_vector,
    const base::flat_set<GURL>& icon_urls_to_download_if_any,
    SizeSet icon_sizes_found,
    bool is_app_icon = false) {
  std::set<SquareSizePx> sizes_to_generate = web_app::SizesToGenerate();

  // There isn't a lot of use-cases where we need to account for the 16x16 icon
  // except for the main app shortcut icon, which is why we also need to
  // generate an extra size of 16x16 if not found in the manifest. This helps
  // handle blurriness of icons to a certain extent because a "closer" size is
  // found for very small sized icons instead of needing to resize a much larger
  // icon. However, this is just a hacky fix, and will be tackled more
  // efficiently once b/322428992 is started.
  if (is_app_icon) {
    sizes_to_generate.emplace(16);
  }

  for (const auto& url : icon_urls_to_download_if_any) {
    for (const auto& width : sizes_to_generate) {
      gfx::Size size_to_generate_in(width, width);

      // Only generate SVG icons for those sizes that are not passed in
      // explicitly via the manifest.
      if (!icon_sizes_found.contains(size_to_generate_in)) {
        icon_vector.push_back(
            IconUrlWithSize::Create(url, size_to_generate_in));
      }
    }
  }
}

std::vector<IconUrlWithSize> GetAppIconUrls(
    const WebAppInstallInfo& web_app_info) {
  std::vector<IconUrlWithSize> urls;

  for (const apps::IconInfo& info : web_app_info.manifest_icons) {
    urls.push_back(IconUrlWithSize::CreateForUnspecifiedSize(info.url));
  }

  PopulateIconUrlsForSizeAnyIfNeeded(
      std::ref(urls),
      GetAllIconUrlsForSizeAny(web_app_info.icons_with_size_any.manifest_icons),
      web_app_info.icons_with_size_any.manifest_icon_provided_sizes,
      /*is_app_icon=*/true);
  return urls;
}

std::vector<IconUrlWithSize> GetShortcutMenuIcons(
    const WebAppInstallInfo& web_app_info) {
  std::vector<IconUrlWithSize> urls;
  for (const WebAppShortcutsMenuItemInfo& shortcut :
       web_app_info.shortcuts_menu_item_infos) {
    for (IconPurpose purpose : kIconPurposes) {
      for (const WebAppShortcutsMenuItemInfo::Icon& icon :
           shortcut.GetShortcutIconInfosForPurpose(purpose)) {
        urls.push_back(IconUrlWithSize::CreateForUnspecifiedSize(icon.url));
      }
    }
  }

  PopulateIconUrlsForSizeAnyIfNeeded(
      std::ref(urls),
      GetAllIconUrlsForSizeAny(
          web_app_info.icons_with_size_any.shortcut_menu_icons),
      web_app_info.icons_with_size_any.shortcut_menu_icons_provided_sizes);
  return urls;
}

std::vector<IconUrlWithSize> GetFileHandlingIcons(
    const WebAppInstallInfo& web_app_info) {
  std::vector<IconUrlWithSize> urls;

  for (const apps::FileHandler& file_handler : web_app_info.file_handlers) {
    for (const apps::IconInfo& icon : file_handler.downloaded_icons) {
      urls.push_back(IconUrlWithSize::CreateForUnspecifiedSize(icon.url));
    }
  }

  PopulateIconUrlsForSizeAnyIfNeeded(
      std::ref(urls),
      GetAllIconUrlsForSizeAny(
          web_app_info.icons_with_size_any.file_handling_icons),
      web_app_info.icons_with_size_any.file_handling_icon_provided_sizes);
  return urls;
}

std::vector<IconUrlWithSize> GetHomeTabIcons(
    const WebAppInstallInfo& web_app_info) {
  std::vector<IconUrlWithSize> urls;

  if (!web_app::HomeTabIconsExistInTabStrip(web_app_info)) {
    return urls;
  }

  const auto& home_tab = absl::get<blink::Manifest::HomeTabParams>(
      web_app_info.tab_strip.value().home_tab);

  for (const auto& icon : home_tab.icons) {
    urls.push_back(IconUrlWithSize::CreateForUnspecifiedSize(icon.src));
  }

  PopulateIconUrlsForSizeAnyIfNeeded(
      std::ref(urls),
      GetAllIconUrlsForSizeAny(web_app_info.icons_with_size_any.home_tab_icons),
      web_app_info.icons_with_size_any.home_tab_icon_provided_sizes);
  return urls;
}

IconUrlSizeSet RemoveDuplicates(std::vector<IconUrlWithSize> from_urls) {
  return IconUrlSizeSet{from_urls};
}

}  // namespace

IconUrlWithSize IconUrlWithSize::CreateForUnspecifiedSize(
    const GURL& icon_url) {
  return IconUrlWithSize(icon_url, gfx::Size());
}

IconUrlWithSize IconUrlWithSize::Create(const GURL& icon_url,
                                        const gfx::Size& size) {
  CHECK(!size.IsZero());
  return IconUrlWithSize(icon_url, size);
}

IconUrlWithSize::IconUrlWithSize(GURL url, gfx::Size size)
    : url(url), size(size) {}

IconUrlWithSize::~IconUrlWithSize() = default;

IconUrlWithSize::IconUrlWithSize(const IconUrlWithSize& icon_urls_with_size) =
    default;

IconUrlWithSize::IconUrlWithSize(IconUrlWithSize&& icon_urls_with_size) =
    default;

IconUrlWithSize& IconUrlWithSize::operator=(
    const IconUrlWithSize& icon_urls_with_size) = default;

bool IconUrlWithSize::operator<(const IconUrlWithSize& rhs) const {
  if (url != rhs.url) {
    return url < rhs.url;
  }

  if (size.width() != rhs.size.width()) {
    return size.width() < rhs.size.width();
  }

  return size.height() < rhs.size.height();
}

bool IconUrlWithSize::operator==(const IconUrlWithSize& rhs) const = default;

std::string IconUrlWithSize::ToString() const {
  return base::StringPrintf("icon_url: %s, size: %s", url.spec().c_str(),
                            size.ToString().c_str());
}

IconUrlSizeSet GetValidIconUrlsToDownload(
    const WebAppInstallInfo& web_app_info) {
  std::vector<IconUrlWithSize> icon_urls_with_sizes;

  base::Extend(icon_urls_with_sizes, GetAppIconUrls(web_app_info));
  base::Extend(icon_urls_with_sizes, GetShortcutMenuIcons(web_app_info));
  base::Extend(icon_urls_with_sizes, GetFileHandlingIcons(web_app_info));
  base::Extend(icon_urls_with_sizes, GetHomeTabIcons(web_app_info));

  return RemoveDuplicates(std::move(icon_urls_with_sizes));
}

}  // namespace web_app
