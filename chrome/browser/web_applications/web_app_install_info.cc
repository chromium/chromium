// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_info.h"

#include <sstream>

#include "chrome/browser/web_applications/web_app_helpers.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "ui/gfx/skia_util.h"

namespace {

template <typename T>
std::string ConvertToString(const T& value) {
  std::stringstream ss;
  ss << value;
  return ss.str();
}

}  // namespace

// This definition doesn't get picked up by IconBitmaps::operator==() when in
// the anonymous namespace but it does as a static free function.
static bool operator==(const SkBitmap& a, const SkBitmap& b) {
  return gfx::BitmapsAreEqual(a, b);
}

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
    icons.Set(ConvertToString(purpose), std::move(purpose_list));
  }
  root.Set("icons", std::move(icons));

  root.Set("downloaded_icons_sizes", downloaded_icon_sizes.AsDebugValue());

  return base::Value(std::move(root));
}

// WebAppInstallInfo

namespace web_app {

// static
WebAppInstallInfo WebAppInstallInfo::CreateInstallInfoForCreateShortcut(
    const GURL& document_url,
    const WebAppInstallInfo& other) {
  WebAppInstallInfo create_shortcut_info(
      web_app::GenerateManifestIdFromStartUrlOnly(document_url));
  create_shortcut_info.title = other.title;
  create_shortcut_info.description = other.description;
  create_shortcut_info.start_url = document_url;
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
  return std::make_unique<WebAppInstallInfo>(
      GenerateManifestIdFromStartUrlOnly(start_url), start_url);
}

WebAppInstallInfo::WebAppInstallInfo() = default;

WebAppInstallInfo::WebAppInstallInfo(const web_app::ManifestId& manifest_id)
    : manifest_id(manifest_id) {
  CHECK(manifest_id.is_valid());
}

WebAppInstallInfo::WebAppInstallInfo(const web_app::ManifestId& manifest_id,
                                     const GURL& start_url)
    : manifest_id(manifest_id), start_url(start_url) {
  CHECK(manifest_id.is_valid());
  CHECK(!manifest_id.has_ref());
  CHECK(start_url.is_valid());
}

WebAppInstallInfo::WebAppInstallInfo(const WebAppInstallInfo& other) = default;

WebAppInstallInfo::WebAppInstallInfo(WebAppInstallInfo&&) = default;

WebAppInstallInfo& WebAppInstallInfo::operator=(WebAppInstallInfo&&) = default;

WebAppInstallInfo::~WebAppInstallInfo() = default;

WebAppInstallInfo WebAppInstallInfo::Clone() const {
  return WebAppInstallInfo(*this);
}

}  // namespace web_app

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
