// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_info.h"

#include <string>
#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/buildflag.h"
#include "chrome/browser/web_applications/model/dialog_image_info.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/origin.h"

namespace web_app {

// WebAppShortcutsMenuItemInfo::Icon
WebAppShortcutsMenuItemInfo::Icon::Icon() = default;

WebAppShortcutsMenuItemInfo::Icon::Icon(
    const WebAppShortcutsMenuItemInfo::Icon&) = default;

WebAppShortcutsMenuItemInfo::Icon::Icon(
    WebAppShortcutsMenuItemInfo::Icon&&) noexcept = default;

WebAppShortcutsMenuItemInfo::Icon::~Icon() = default;

WebAppShortcutsMenuItemInfo::Icon& WebAppShortcutsMenuItemInfo::Icon::operator=(
    const WebAppShortcutsMenuItemInfo::Icon&) = default;

WebAppShortcutsMenuItemInfo::Icon& WebAppShortcutsMenuItemInfo::Icon::operator=(
    WebAppShortcutsMenuItemInfo::Icon&&) = default;

base::Value WebAppShortcutsMenuItemInfo::Icon::AsDebugValue() const {
  base::DictValue root;
  root.Set("url", url.spec());
  root.Set("square_size_px", square_size_px);
  return base::Value(std::move(root));
}

// WebAppShortcutsMenuItemInfo
WebAppShortcutsMenuItemInfo::WebAppShortcutsMenuItemInfo() = default;

WebAppShortcutsMenuItemInfo::WebAppShortcutsMenuItemInfo(
    const WebAppShortcutsMenuItemInfo& other) = default;

WebAppShortcutsMenuItemInfo::WebAppShortcutsMenuItemInfo(
    WebAppShortcutsMenuItemInfo&&) noexcept = default;

WebAppShortcutsMenuItemInfo::~WebAppShortcutsMenuItemInfo() = default;

WebAppShortcutsMenuItemInfo& WebAppShortcutsMenuItemInfo::operator=(
    const WebAppShortcutsMenuItemInfo&) = default;

WebAppShortcutsMenuItemInfo& WebAppShortcutsMenuItemInfo::operator=(
    WebAppShortcutsMenuItemInfo&&) noexcept = default;

const std::vector<WebAppShortcutsMenuItemInfo::Icon>&
WebAppShortcutsMenuItemInfo::GetShortcutIconInfosForPurpose(
    IconPurpose purpose) const {
  switch (purpose) {
    case IconPurpose::MONOCHROME:
      return monochrome;
    case IconPurpose::ANY:
      return any;
    case IconPurpose::MASKABLE:
      return maskable;
  }
}

void WebAppShortcutsMenuItemInfo::SetShortcutIconInfosForPurpose(
    IconPurpose purpose,
    std::vector<WebAppShortcutsMenuItemInfo::Icon> shortcut_manifest_icons) {
  switch (purpose) {
    case IconPurpose::ANY:
      any = std::move(shortcut_manifest_icons);
      return;
    case IconPurpose::MONOCHROME:
      monochrome = std::move(shortcut_manifest_icons);
      return;
    case IconPurpose::MASKABLE:
      maskable = std::move(shortcut_manifest_icons);
      return;
  }
}

base::Value WebAppShortcutsMenuItemInfo::AsDebugValue() const {
  TRACE_EVENT0("ui", "WebAppShortcutsMenuItemInfo::AsDebugValue");
  base::DictValue root;

  root.Set("name", name);

  root.Set("url", url.spec());

  base::DictValue icons;
  for (IconPurpose purpose : kIconPurposes) {
    base::ListValue purpose_list;
    for (const WebAppShortcutsMenuItemInfo::Icon& icon :
         GetShortcutIconInfosForPurpose(purpose)) {
      purpose_list.Append(icon.AsDebugValue());
    }
    icons.Set(base::ToString(purpose), std::move(purpose_list));
  }
  root.Set("icons", std::move(icons));

  root.Set("downloaded_icons_sizes", downloaded_icon_sizes.AsDebugValue());

  return base::Value(std::move(root));
}

// IconsWithSizeAny
IconsWithSizeAny::IconsWithSizeAny() = default;
IconsWithSizeAny::~IconsWithSizeAny() = default;
IconsWithSizeAny::IconsWithSizeAny(
    const IconsWithSizeAny& icons_with_size_any) = default;
IconsWithSizeAny& IconsWithSizeAny::operator=(
    const IconsWithSizeAny& icons_with_size_any) = default;
bool IconsWithSizeAny::operator==(
    const IconsWithSizeAny& icons_with_size_any) const = default;

base::Value IconsWithSizeAny::ToDebugValue() const {
  base::DictValue icons;
  base::DictValue manifest;
  for (const auto& icon : manifest_icons) {
    manifest.Set(base::ToString(icon.first), icon.second.spec());
  }
  icons.Set("manifest_icons", base::Value(std::move(manifest)));
  base::ListValue manifest_sizes;
  for (const auto& size : manifest_icon_provided_sizes) {
    manifest_sizes.Append(size.ToString());
  }
  icons.Set("manifest_provided_sizes", base::Value(std::move(manifest_sizes)));

  base::DictValue shortcut_icon;
  for (const auto& shicon : shortcut_menu_icons) {
    shortcut_icon.Set(base::ToString(shicon.first), shicon.second.spec());
  }
  icons.Set("shortcut_icons", base::Value(std::move(shortcut_icon)));
  base::ListValue shortcut_sizes;
  for (const auto& size : shortcut_menu_icons_provided_sizes) {
    shortcut_sizes.Append(size.ToString());
  }
  icons.Set("shortcut_menu_icons_provided_sizes",
            base::Value(std::move(shortcut_sizes)));

  base::DictValue file_handlers;
  for (const auto& fhicon : file_handling_icons) {
    file_handlers.Set(base::ToString(fhicon.first), fhicon.second.spec());
  }
  icons.Set("file_handling_icons", base::Value(std::move(file_handlers)));
  base::ListValue file_handling_sizes;
  for (const auto& size : file_handling_icon_provided_sizes) {
    file_handling_sizes.Append(size.ToString());
  }
  icons.Set("file_handling_icons_manifest_provided_sizes",
            base::Value(std::move(file_handling_sizes)));

  base::DictValue tab_icons;
  for (const auto& thicon : home_tab_icons) {
    tab_icons.Set(base::ToString(thicon.first), thicon.second.spec());
  }
  icons.Set("home_tab_icons", base::Value(std::move(tab_icons)));
  base::ListValue home_tab_sizes;
  for (const auto& size : home_tab_icon_provided_sizes) {
    home_tab_sizes.Append(size.ToString());
  }
  icons.Set("home_tab_icons_manifest_provided_sizes",
            base::Value(std::move(home_tab_sizes)));

  return base::Value(std::move(icons));
}

std::string IconsWithSizeAny::ToString() const {
  return ToDebugValue().DebugString();
}

// WebAppInstallInfo

// static
std::unique_ptr<WebAppInstallInfo>
WebAppInstallInfo::CreateWithStartUrlForTesting(const GURL& start_url) {
  CHECK_IS_TEST();
  auto info = std::make_unique<WebAppInstallInfo>(
      GenerateManifestIdFromStartUrlOnly(start_url), start_url);
  info->scope = start_url.GetWithoutFilename();
  CHECK(!info->scope.is_empty());
  return info;
}

// static
std::unique_ptr<WebAppInstallInfo> WebAppInstallInfo::CreateForTesting(
    const GURL& start_url,
    blink::mojom::DisplayMode display,
    mojom::UserDisplayMode user_mode,
    std::optional<blink::mojom::ManifestLaunchHandler_ClientMode> client_mode) {
  CHECK_IS_TEST();
  auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  info->title = base::ASCIIToUTF16(start_url.PathForRequest());
  info->display_mode = display;
  info->user_display_mode = user_mode;
  info->launch_handler = blink::Manifest::LaunchHandler(client_mode);
  CHECK_EQ(info->launch_handler->client_mode_valid_and_specified(),
           client_mode.has_value());
  CHECK(!info->scope.is_empty());
  return info;
}

// static
base::expected<WebAppInstallInfo, std::string> WebAppInstallInfo::Create(
    const GURL& manifest_url,
    const webapps::ManifestId& manifest_id,
    const GURL& start_url) {
  if (!manifest_id.is_valid()) {
    return base::unexpected(
        "Manifest `id` is not present or invalid. manifest_url: " +
        manifest_url.possibly_invalid_spec());
  }
  if (!start_url.is_valid()) {
    return base::unexpected(
        "Manifest `start_url` is not present or invalid. manifest_url: " +
        manifest_url.possibly_invalid_spec());
  }
  if (!url::Origin::Create(start_url).IsSameOriginWith(
          url::Origin::Create(manifest_id))) {
    return base::unexpected(
        "Manifest `id` and `start_url` must have the same origin. "
        "manifest_url: " +
        manifest_url.possibly_invalid_spec());
  }

  WebAppInstallInfo info(manifest_id, start_url);
  info.scope = start_url.GetWithoutFilename();
  CHECK(!info.scope.is_empty());
  return info;
}

namespace {
void CheckValidManifestIdAndStartUrl(const webapps::ManifestId& manifest_id,
                                     const GURL& start_url) {
  CHECK(manifest_id.is_valid());
  CHECK(!manifest_id.has_ref());
  CHECK(start_url.is_valid());
  CHECK(url::Origin::Create(start_url).IsSameOriginWith(
      url::Origin::Create(manifest_id)));
}
}  // namespace

WebAppInstallInfo::WebAppInstallInfo(const webapps::ManifestId& manifest_id,
                                     const GURL& start_url)
    : scope(start_url.GetWithoutFilename()),
      manifest_id_(manifest_id),
      start_url_(start_url) {
  CheckValidManifestIdAndStartUrl(manifest_id_, start_url_);
}

WebAppInstallInfo::WebAppInstallInfo(const WebAppInstallInfo& other) = default;

WebAppInstallInfo::WebAppInstallInfo(WebAppInstallInfo&&) = default;

WebAppInstallInfo& WebAppInstallInfo::operator=(WebAppInstallInfo&&) = default;

WebAppInstallInfo::~WebAppInstallInfo() = default;

WebAppInstallInfo WebAppInstallInfo::Clone() const {
  return WebAppInstallInfo(*this);
}

void WebAppInstallInfo::SetManifestIdAndStartUrl(
    const webapps::ManifestId& manifest_id,
    const GURL& start_url) {
  CheckValidManifestIdAndStartUrl(manifest_id, start_url);
  manifest_id_ = manifest_id;
  start_url_ = start_url;
}

DialogImageInfo WebAppInstallInfo::GetIconBitmapsForSecureSurfaces() const {
  DialogImageInfo image_info;
  if (trusted_icon_bitmaps.empty()) {
    image_info.bitmaps = icon_bitmaps.any;
    return image_info;
  }

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
  if (!trusted_icon_bitmaps.maskable.empty()) {
    image_info.bitmaps = trusted_icon_bitmaps.maskable;
    image_info.is_maskable = true;
    return image_info;
  }
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)

  image_info.bitmaps = trusted_icon_bitmaps.any;
  return image_info;
}

bool operator==(const WebAppShortcutsMenuItemInfo::Icon& icon1,
                const WebAppShortcutsMenuItemInfo::Icon& icon2) {
  return std::tie(icon1.url, icon1.square_size_px) ==
         std::tie(icon2.url, icon2.square_size_px);
}

bool operator==(const WebAppShortcutsMenuItemInfo& shortcut_info1,
                const WebAppShortcutsMenuItemInfo& shortcut_info2) {
  return std::tie(shortcut_info1.name, shortcut_info1.url, shortcut_info1.any,
                  shortcut_info1.maskable, shortcut_info1.monochrome) ==
         std::tie(shortcut_info2.name, shortcut_info2.url, shortcut_info2.any,
                  shortcut_info2.maskable, shortcut_info2.monochrome);
}

}  // namespace web_app
