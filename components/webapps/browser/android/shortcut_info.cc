// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/shortcut_info.h"

#include <optional>
#include <string>

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "build/android_buildflags.h"
#include "components/webapps/browser/android/webapps_icon_utils.h"
#include "components/webapps/browser/features.h"
#include "shortcut_info.h"
#include "third_party/blink/public/common/manifest/manifest_icon_selector.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace webapps {

namespace {

// The maximum number of shortcuts an Android launcher supports.
// https://developer.android.com/guide/topics/ui/shortcuts#shortcut-limitations
constexpr size_t kMaxShortcuts = 4;

bool IsWebApkDisplayMode(blink::mojom::DisplayMode display_mode) {
  return (display_mode == blink::mojom::DisplayMode::kStandalone ||
          display_mode == blink::mojom::DisplayMode::kFullscreen ||
          display_mode == blink::mojom::DisplayMode::kMinimalUi);
}

}  // namespace

using blink::mojom::DisplayMode;

ShareTargetParamsFile::ShareTargetParamsFile() = default;

ShareTargetParamsFile::ShareTargetParamsFile(
    const ShareTargetParamsFile& other) = default;

ShareTargetParamsFile::~ShareTargetParamsFile() = default;

ShareTargetParams::ShareTargetParams() = default;

ShareTargetParams::ShareTargetParams(const ShareTargetParams& other) = default;

ShareTargetParams::~ShareTargetParams() = default;

ShareTarget::ShareTarget() = default;

ShareTarget::~ShareTarget() = default;

ShortcutInfo::ShortcutInfo(const GURL& shortcut_url)
    : url(shortcut_url),
      scope(shortcut_url.GetWithoutFilename()),
      manifest_id(shortcut_url) {}

ShortcutInfo::ShortcutInfo(const ShortcutInfo& other) = default;

ShortcutInfo::~ShortcutInfo() = default;

// static
std::unique_ptr<ShortcutInfo> ShortcutInfo::CreateShortcutInfo(
    const GURL& url,
    const GURL& manifest_url,
    const blink::mojom::Manifest& manifest,
    const mojom::WebPageMetadata& web_page_metadata,
    const GURL& primary_icon_url,
    bool primary_icon_maskable) {
  auto shortcut_info = std::make_unique<ShortcutInfo>(url);
  shortcut_info->UpdateFromWebPageMetadata(web_page_metadata);
  shortcut_info->UpdateFromManifest(manifest);
  shortcut_info->manifest_url = manifest_url;
  shortcut_info->best_primary_icon_url = primary_icon_url;
  shortcut_info->is_primary_icon_maskable = primary_icon_maskable;
  shortcut_info->UpdateBestSplashIcon(manifest);
  return shortcut_info;
}

std::map<GURL, std::unique_ptr<WebappIcon>> ShortcutInfo::GetWebApkIcons()
    const {
  std::map<GURL, std::unique_ptr<WebappIcon>> icons;
  if (best_primary_icon_url.is_valid()) {
    icons.emplace(best_primary_icon_url,
                  std::make_unique<WebappIcon>(best_primary_icon_url,
                                               is_primary_icon_maskable,
                                               webapk::Image::PRIMARY_ICON));
  }

  if (splash_image_url.is_valid()) {
    auto it = icons.find(splash_image_url);
    if (it != icons.end()) {
      it->second->AddUsage(webapk::Image::SPLASH_ICON);
    } else {
      icons.emplace(splash_image_url,
                    std::make_unique<WebappIcon>(splash_image_url,
                                                 is_splash_image_maskable,
                                                 webapk::Image::SPLASH_ICON));
    }
  }

  for (const auto& shortcut_icon_url : best_shortcut_icon_urls) {
    auto it = icons.find(shortcut_icon_url);
    if (it != icons.end()) {
      it->second->AddUsage(webapk::Image::SHORTCUT_ICON);
    } else {
      icons.emplace(shortcut_icon_url,
                    std::make_unique<WebappIcon>(shortcut_icon_url, false,
                                                 webapk::Image::SHORTCUT_ICON));
    }
  }

  return icons;
}

void ShortcutInfo::UpdateFromWebPageMetadata(
    const mojom::WebPageMetadata& metadata) {
  std::u16string title;
  base::TrimWhitespace(metadata.title, base::TrimPositions::TRIM_ALL, &title);
  if (!title.empty()) {
    user_title = title;
  }
  std::u16string app_name;
  base::TrimWhitespace(metadata.application_name, base::TrimPositions::TRIM_ALL,
                       &app_name);
  if (!app_name.empty()) {
    user_title = app_name;
  }
  short_name = user_title;
  name = user_title;

  if (!metadata.description.empty()) {
    description = metadata.description;
  }
  if (metadata.application_url.is_valid()) {
    url = metadata.application_url;
    scope = metadata.application_url.GetWithoutFilename();
  }
  if (metadata.mobile_capable == mojom::WebPageMobileCapable::ENABLED ||
      metadata.mobile_capable == mojom::WebPageMobileCapable::ENABLED_APPLE) {
    display = blink::mojom::DisplayMode::kStandalone;
  }
}

void ShortcutInfo::UpdateFromManifest(const blink::mojom::Manifest& manifest) {
  if (blink::IsEmptyManifest(manifest)) {
    return;
  }
  std::u16string s_name = manifest.short_name.value_or(std::u16string());
  std::u16string f_name = manifest.name.value_or(std::u16string());
  if (!s_name.empty() || !f_name.empty()) {
    short_name = s_name;
    name = f_name;
    if (short_name.empty())
      short_name = name;
    else if (name.empty())
      name = short_name;
    user_title = short_name;
  }

  if (manifest.description.has_value()) {
    description = manifest.description.value();
  }

  // Set the url based on the manifest value, if any.
  if (manifest.start_url.is_valid()) {
    url = manifest.start_url;
  }

  if (manifest.scope.is_valid()) {
    scope = manifest.scope;
  }

  if (manifest.id.is_valid()) {
    manifest_id = manifest.id;
  }

  // Set the display based on the manifest value, if any.
  if (manifest.display != DisplayMode::kUndefined)
    display = manifest.display;

  for (DisplayMode display_mode : manifest.display_override) {
    if (display_mode == DisplayMode::kBrowser ||
        display_mode == DisplayMode::kMinimalUi ||
        display_mode == DisplayMode::kStandalone ||
        display_mode == DisplayMode::kFullscreen) {
      display = display_mode;
      break;
    }
  }

  if (display == DisplayMode::kStandalone ||
      display == DisplayMode::kFullscreen ||
      display == DisplayMode::kMinimalUi) {
    // Set the orientation based on the manifest value, or ignore if the display
    // mode is different from 'standalone', 'fullscreen' or 'minimal-ui'.
    if (manifest.orientation !=
        device::mojom::ScreenOrientationLockType::DEFAULT) {
      // TODO(mlamouri): Send a message to the developer console if we ignored
      // Manifest orientation because display property is not set.
      orientation = manifest.orientation;
    }
  }

  // Set the theme color based on the manifest value, if any.
  theme_color = manifest.has_theme_color
                    ? std::make_optional(manifest.theme_color)
                    : std::nullopt;

  // Set the background color based on the manifest value, if any.
  background_color = manifest.has_background_color
                         ? std::make_optional(manifest.background_color)
                         : std::nullopt;

  // Set the icon urls based on the icons in the manifest, if any.
  icon_urls.clear();
  for (const auto& icon : manifest.icons)
    icon_urls.push_back(icon.src.spec());

  // Set the screenshots urls based on the screenshots in the manifest, if any.
  screenshot_urls.clear();
  for (const auto& screenshot : manifest.screenshots)
    screenshot_urls.push_back(screenshot->image.src);

  if (manifest.share_target) {
    share_target = ShareTarget();
    share_target->action = manifest.share_target->action;
    share_target->method = manifest.share_target->method;
    share_target->enctype = manifest.share_target->enctype;
    if (manifest.share_target->params.text)
      share_target->params.text = *manifest.share_target->params.text;
    if (manifest.share_target->params.title)
      share_target->params.title = *manifest.share_target->params.title;
    if (manifest.share_target->params.url)
      share_target->params.url = *manifest.share_target->params.url;

    for (blink::Manifest::FileFilter manifest_share_target_file :
         manifest.share_target->params.files) {
      ShareTargetParamsFile share_target_params_file;
      share_target_params_file.name = manifest_share_target_file.name;
      share_target_params_file.accept = manifest_share_target_file.accept;
      share_target->params.files.push_back(share_target_params_file);
    }
  }

  shortcut_items = manifest.shortcuts;
  if (shortcut_items.size() > kMaxShortcuts)
    shortcut_items.resize(kMaxShortcuts);

  for (auto& shortcut_item : shortcut_items) {
    if (!shortcut_item.short_name || shortcut_item.short_name->empty())
      shortcut_item.short_name = shortcut_item.name;
  }

  int ideal_shortcut_icons_size_px =
      WebappsIconUtils::GetIdealShortcutIconSizeInPx();
  for (const auto& manifest_shortcut : shortcut_items) {
    GURL best_url = blink::ManifestIconSelector::FindBestMatchingSquareIcon(
        manifest_shortcut.icons, ideal_shortcut_icons_size_px,
        /* minimum_icon_size_in_px= */ ideal_shortcut_icons_size_px / 2,
        blink::mojom::ManifestImageResource_Purpose::ANY);
    best_shortcut_icon_urls.push_back(std::move(best_url));
  }

  // Set the dark theme color based on the manifest value, if any.
  dark_theme_color = manifest.has_dark_theme_color
                         ? std::make_optional(manifest.dark_theme_color)
                         : std::nullopt;

  // Set the dark background color based on the manifest value, if any.
  dark_background_color =
      manifest.has_dark_background_color
          ? std::make_optional(manifest.dark_background_color)
          : std::nullopt;
}

void ShortcutInfo::UpdateBestSplashIcon(
    const blink::mojom::Manifest& manifest) {
  ideal_splash_image_size_in_px =
      WebappsIconUtils::GetIdealSplashImageSizeInPx();
  minimum_splash_image_size_in_px =
      WebappsIconUtils::GetMinimumSplashImageSizeInPx();

  if (WebappsIconUtils::DoesAndroidSupportMaskableIcons()) {
    splash_image_url = blink::ManifestIconSelector::FindBestMatchingSquareIcon(
        manifest.icons, ideal_splash_image_size_in_px,
        minimum_splash_image_size_in_px,
        blink::mojom::ManifestImageResource_Purpose::MASKABLE);
    is_splash_image_maskable = true;
  }
  // If did not fetch maskable icon for splash image, or can not find a best
  // match, fallback to ANY icon.
  if (!splash_image_url.is_valid()) {
    splash_image_url = blink::ManifestIconSelector::FindBestMatchingSquareIcon(
        manifest.icons, ideal_splash_image_size_in_px,
        minimum_splash_image_size_in_px,
        blink::mojom::ManifestImageResource_Purpose::ANY);
    is_splash_image_maskable = false;
  }
}

void ShortcutInfo::UpdateDisplayMode(bool webapk_compatible) {
#if BUILDFLAG(IS_DESKTOP_ANDROID)
  constexpr bool is_desktop_android = true;
#else
  constexpr bool is_desktop_android = false;
#endif

  if (webapk_compatible) {
    if (!IsWebApkDisplayMode(display)) {
      display = DisplayMode::kMinimalUi;
    }
  } else if (is_desktop_android) {
    if (!IsWebApkDisplayMode(display)) {
      display = DisplayMode::kStandalone;
    }
  } else {
    if (IsWebApkDisplayMode(display)) {
      display = DisplayMode::kMinimalUi;
    } else {
      display = DisplayMode::kBrowser;
    }
  }
}

}  // namespace webapps
