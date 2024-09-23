// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_info.h"

#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/containers/flat_tree.h"
#include "base/not_fatal_until.h"
#include "base/strings/to_string.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "ui/gfx/skia_util.h"
#include "url/origin.h"

// This definition needs to be in the top-level namespace to be picked up by
// IconBitmaps::operator==().
static bool operator==(const SkBitmap& a, const SkBitmap& b) {
  return gfx::BitmapsAreEqual(a, b);
}

namespace web_app {

apps::IconInfo::Purpose ManifestPurposeToIconInfoPurpose(
    IconPurpose manifest_purpose) {
  switch (manifest_purpose) {
    case IconPurpose::ANY:
      return apps::IconInfo::Purpose::kAny;
    case IconPurpose::MONOCHROME:
      return apps::IconInfo::Purpose::kMonochrome;
    case IconPurpose::MASKABLE:
      return apps::IconInfo::Purpose::kMaskable;
  }
}

// IconBitmaps
IconBitmaps::IconBitmaps() = default;

IconBitmaps::~IconBitmaps() = default;

IconBitmaps::IconBitmaps(const IconBitmaps&) = default;

IconBitmaps::IconBitmaps(IconBitmaps&&) noexcept = default;

IconBitmaps& IconBitmaps::operator=(const IconBitmaps&) = default;

IconBitmaps& IconBitmaps::operator=(IconBitmaps&&) noexcept = default;

bool IconBitmaps::operator==(const IconBitmaps& other) const {
  auto AsTuple = [](const IconBitmaps& icon_bitmaps) {
    return std::make_tuple(icon_bitmaps.any, icon_bitmaps.maskable,
                           icon_bitmaps.monochrome);
  };
  return AsTuple(*this) == AsTuple(other);
}

const std::map<SquareSizePx, SkBitmap>& IconBitmaps::GetBitmapsForPurpose(
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

void IconBitmaps::SetBitmapsForPurpose(
    IconPurpose purpose,
    std::map<SquareSizePx, SkBitmap> bitmaps) {
  switch (purpose) {
    case IconPurpose::ANY:
      any = std::move(bitmaps);
      return;
    case IconPurpose::MONOCHROME:
      monochrome = std::move(bitmaps);
      return;
    case IconPurpose::MASKABLE:
      maskable = std::move(bitmaps);
      return;
  }
}

bool IconBitmaps::empty() const {
  return any.empty() && maskable.empty() && monochrome.empty();
}

// IconSizes
IconSizes::IconSizes() = default;

IconSizes::~IconSizes() = default;

IconSizes::IconSizes(const IconSizes&) = default;

IconSizes::IconSizes(IconSizes&&) noexcept = default;

IconSizes& IconSizes::operator=(const IconSizes&) = default;

IconSizes& IconSizes::operator=(IconSizes&&) noexcept = default;

base::Value IconSizes::AsDebugValue() const {
  auto ConvertList = [](const auto& list) {
    base::Value::List list_json;
    for (const auto& item : list) {
      list_json.Append(item);
    }
    return list_json;
  };

  base::Value::Dict root;
  for (IconPurpose purpose : kIconPurposes) {
    root.Set(base::ToString(purpose), ConvertList(GetSizesForPurpose(purpose)));
  }

  return base::Value(std::move(root));
}

const std::vector<SquareSizePx>& IconSizes::GetSizesForPurpose(
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

void IconSizes::SetSizesForPurpose(IconPurpose purpose,
                                   std::vector<SquareSizePx> sizes) {
  switch (purpose) {
    case IconPurpose::ANY:
      any = std::move(sizes);
      return;
    case IconPurpose::MONOCHROME:
      monochrome = std::move(sizes);
      return;
    case IconPurpose::MASKABLE:
      maskable = std::move(sizes);
      return;
  }
}

bool IconSizes::empty() const {
  return any.empty() && maskable.empty() && monochrome.empty();
}

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
  base::Value::Dict root;
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
  base::Value::Dict root;

  root.Set("name", name);

  root.Set("url", url.spec());

  base::Value::Dict icons;
  for (IconPurpose purpose : kIconPurposes) {
    base::Value::List purpose_list;
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
  base::Value::Dict icons;
  base::Value::Dict manifest;
  for (const auto& icon : manifest_icons) {
    manifest.Set(base::ToString(icon.first), icon.second.spec());
  }
  icons.Set("manifest_icons", base::Value(std::move(manifest)));
  base::Value::List manifest_sizes;
  for (const auto& size : manifest_icon_provided_sizes) {
    manifest_sizes.Append(size.ToString());
  }
  icons.Set("manifest_provided_sizes", base::Value(std::move(manifest_sizes)));

  base::Value::Dict shortcut_icon;
  for (const auto& shicon : shortcut_menu_icons) {
    shortcut_icon.Set(base::ToString(shicon.first), shicon.second.spec());
  }
  icons.Set("shortcut_icons", base::Value(std::move(shortcut_icon)));
  base::Value::List shortcut_sizes;
  for (const auto& size : shortcut_menu_icons_provided_sizes) {
    shortcut_sizes.Append(size.ToString());
  }
  icons.Set("shortcut_menu_icons_provided_sizes",
            base::Value(std::move(shortcut_sizes)));

  base::Value::Dict file_handlers;
  for (const auto& fhicon : file_handling_icons) {
    file_handlers.Set(base::ToString(fhicon.first), fhicon.second.spec());
  }
  icons.Set("file_handling_icons", base::Value(std::move(file_handlers)));
  base::Value::List file_handling_sizes;
  for (const auto& size : file_handling_icon_provided_sizes) {
    file_handling_sizes.Append(size.ToString());
  }
  icons.Set("file_handling_icons_manifest_provided_sizes",
            base::Value(std::move(file_handling_sizes)));

  base::Value::Dict tab_icons;
  for (const auto& thicon : home_tab_icons) {
    tab_icons.Set(base::ToString(thicon.first), thicon.second.spec());
  }
  icons.Set("home_tab_icons", base::Value(std::move(tab_icons)));
  base::Value::List home_tab_sizes;
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
WebAppInstallInfo WebAppInstallInfo::CreateInstallInfoForCreateShortcut(
    const GURL& document_url,
    const std::u16string& document_title,
    const WebAppInstallInfo& other) {
  WebAppInstallInfo create_shortcut_info(
      GenerateManifestIdFromStartUrlOnly(document_url), document_url);
  create_shortcut_info.title = document_title;
  create_shortcut_info.description = other.description;
  create_shortcut_info.manifest_url = other.manifest_url;
  create_shortcut_info.manifest_icons = other.manifest_icons;
  create_shortcut_info.icon_bitmaps = other.icon_bitmaps;
  create_shortcut_info.other_icon_bitmaps = other.other_icon_bitmaps;
  create_shortcut_info.is_generated_icon = other.is_generated_icon;
  create_shortcut_info.theme_color = other.theme_color;
  create_shortcut_info.dark_mode_theme_color = other.dark_mode_theme_color;
  create_shortcut_info.background_color = other.background_color;
  create_shortcut_info.dark_mode_background_color =
      other.dark_mode_background_color;
  create_shortcut_info.display_mode = other.display_mode;
  create_shortcut_info.display_override = other.display_override;
  create_shortcut_info.additional_search_terms = other.additional_search_terms;
  create_shortcut_info.install_url = other.install_url;
  return create_shortcut_info;
}

// static
std::unique_ptr<WebAppInstallInfo>
WebAppInstallInfo::CreateWithStartUrlForTesting(const GURL& start_url) {
  auto info = std::make_unique<WebAppInstallInfo>(
      GenerateManifestIdFromStartUrlOnly(start_url), start_url);
  info->scope = start_url.GetWithoutFilename();
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

  return WebAppInstallInfo(manifest_id, start_url);
}

namespace {
void CheckValidManifestIdAndStartUrl(const webapps::ManifestId& manifest_id,
                                     const GURL& start_url) {
  CHECK(manifest_id.is_valid(), base::NotFatalUntil::M129);
  CHECK(!manifest_id.has_ref(), base::NotFatalUntil::M129);
  CHECK(start_url.is_valid(), base::NotFatalUntil::M129);
  CHECK(url::Origin::Create(start_url).IsSameOriginWith(
            url::Origin::Create(manifest_id)),
        base::NotFatalUntil::M129);
}
}  // namespace

WebAppInstallInfo::WebAppInstallInfo(const webapps::ManifestId& manifest_id,
                                     const GURL& start_url)
    : manifest_id_(manifest_id), start_url_(start_url) {
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

bool operator==(const IconSizes& icon_sizes1, const IconSizes& icon_sizes2) {
  return std::tie(icon_sizes1.any, icon_sizes1.maskable,
                  icon_sizes1.monochrome) == std::tie(icon_sizes2.any,
                                                      icon_sizes2.maskable,
                                                      icon_sizes2.monochrome);
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
