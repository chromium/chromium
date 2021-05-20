// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_application_info.h"

#include "components/webapps/common/web_page_metadata.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

// IconBitmaps
IconBitmaps::IconBitmaps() = default;

IconBitmaps::~IconBitmaps() = default;

IconBitmaps::IconBitmaps(const IconBitmaps&) = default;

IconBitmaps::IconBitmaps(IconBitmaps&&) = default;

IconBitmaps& IconBitmaps::operator=(const IconBitmaps&) = default;

IconBitmaps& IconBitmaps::operator=(IconBitmaps&&) = default;

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

IconSizes::IconSizes(IconSizes&&) = default;

IconSizes& IconSizes::operator=(const IconSizes&) = default;

IconSizes& IconSizes::operator=(IconSizes&&) = default;

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

// WebApplicationIconInfo
WebApplicationIconInfo::WebApplicationIconInfo() = default;
WebApplicationIconInfo::WebApplicationIconInfo(const GURL& url,
                                               SquareSizePx size)
    : url(url), square_size_px(size) {}

WebApplicationIconInfo::WebApplicationIconInfo(const WebApplicationIconInfo&) =
    default;

WebApplicationIconInfo::WebApplicationIconInfo(WebApplicationIconInfo&&) =
    default;

WebApplicationIconInfo::~WebApplicationIconInfo() = default;

WebApplicationIconInfo& WebApplicationIconInfo::operator=(
    const WebApplicationIconInfo&) = default;

WebApplicationIconInfo& WebApplicationIconInfo::operator=(
    WebApplicationIconInfo&&) = default;

// WebApplicationShortcutsMenuItemInfo::Icon
WebApplicationShortcutsMenuItemInfo::Icon::Icon() = default;

WebApplicationShortcutsMenuItemInfo::Icon::Icon(
    const WebApplicationShortcutsMenuItemInfo::Icon&) = default;

WebApplicationShortcutsMenuItemInfo::Icon::Icon(
    WebApplicationShortcutsMenuItemInfo::Icon&&) = default;

WebApplicationShortcutsMenuItemInfo::Icon::~Icon() = default;

WebApplicationShortcutsMenuItemInfo::Icon&
WebApplicationShortcutsMenuItemInfo::Icon::operator=(
    const WebApplicationShortcutsMenuItemInfo::Icon&) = default;

WebApplicationShortcutsMenuItemInfo::Icon&
WebApplicationShortcutsMenuItemInfo::Icon::operator=(
    WebApplicationShortcutsMenuItemInfo::Icon&&) = default;

// WebApplicationShortcutsMenuItemInfo
WebApplicationShortcutsMenuItemInfo::WebApplicationShortcutsMenuItemInfo() =
    default;

WebApplicationShortcutsMenuItemInfo::WebApplicationShortcutsMenuItemInfo(
    const WebApplicationShortcutsMenuItemInfo& other) = default;

WebApplicationShortcutsMenuItemInfo::WebApplicationShortcutsMenuItemInfo(
    WebApplicationShortcutsMenuItemInfo&&) noexcept = default;

WebApplicationShortcutsMenuItemInfo::~WebApplicationShortcutsMenuItemInfo() =
    default;

WebApplicationShortcutsMenuItemInfo&
WebApplicationShortcutsMenuItemInfo::operator=(
    const WebApplicationShortcutsMenuItemInfo&) = default;

WebApplicationShortcutsMenuItemInfo&
WebApplicationShortcutsMenuItemInfo::operator=(
    WebApplicationShortcutsMenuItemInfo&&) noexcept = default;

const std::vector<WebApplicationShortcutsMenuItemInfo::Icon>&
WebApplicationShortcutsMenuItemInfo::GetShortcutIconInfosForPurpose(
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

void WebApplicationShortcutsMenuItemInfo::SetShortcutIconInfosForPurpose(
    IconPurpose purpose,
    std::vector<WebApplicationShortcutsMenuItemInfo::Icon>
        shortcut_icon_infos) {
  switch (purpose) {
    case IconPurpose::ANY:
      any = std::move(shortcut_icon_infos);
      return;
    case IconPurpose::MONOCHROME:
      monochrome = std::move(shortcut_icon_infos);
      return;
    case IconPurpose::MASKABLE:
      maskable = std::move(shortcut_icon_infos);
      return;
  }
}

// WebApplicationInfo
WebApplicationInfo::WebApplicationInfo() = default;

WebApplicationInfo::WebApplicationInfo(const WebApplicationInfo& other) =
    default;

WebApplicationInfo::WebApplicationInfo(
    const webapps::mojom::WebPageMetadata& metadata)
    : title(metadata.application_name),
      description(metadata.description),
      start_url(metadata.application_url) {
  for (const auto& icon : metadata.icons) {
    WebApplicationIconInfo icon_info;
    icon_info.url = icon->url;
    if (icon->square_size_px > 0)
      icon_info.square_size_px = icon->square_size_px;
    icon_infos.push_back(icon_info);
  }
  switch (metadata.mobile_capable) {
    case webapps::mojom::WebPageMobileCapable::UNSPECIFIED:
      mobile_capable = MOBILE_CAPABLE_UNSPECIFIED;
      break;
    case webapps::mojom::WebPageMobileCapable::ENABLED:
      mobile_capable = MOBILE_CAPABLE;
      break;
    case webapps::mojom::WebPageMobileCapable::ENABLED_APPLE:
      mobile_capable = MOBILE_CAPABLE_APPLE;
      break;
  }
}

WebApplicationInfo::~WebApplicationInfo() = default;

bool operator==(const WebApplicationIconInfo& icon_info1,
                const WebApplicationIconInfo& icon_info2) {
  return std::tie(icon_info1.url, icon_info1.square_size_px,
                  icon_info1.purpose) == std::tie(icon_info2.url,
                                                  icon_info2.square_size_px,
                                                  icon_info2.purpose);
}

std::ostream& operator<<(std::ostream& out,
                         const WebApplicationIconInfo& icon_info) {
  out << "url: " << icon_info.url << std::endl;
  out << "  square_size_px: ";
  if (icon_info.square_size_px)
    out << *icon_info.square_size_px << std::endl;
  else
    out << "none" << std::endl;
  out << "  purpose: " << icon_info.purpose << std::endl;
  return out;
}

bool operator==(const IconSizes& icon_sizes1, const IconSizes& icon_sizes2) {
  return std::tie(icon_sizes1.any, icon_sizes1.maskable,
                  icon_sizes1.monochrome) == std::tie(icon_sizes2.any,
                                                      icon_sizes2.maskable,
                                                      icon_sizes2.monochrome);
}

bool operator==(const WebApplicationShortcutsMenuItemInfo::Icon& icon1,
                const WebApplicationShortcutsMenuItemInfo::Icon& icon2) {
  return std::tie(icon1.url, icon1.square_size_px) ==
         std::tie(icon2.url, icon2.square_size_px);
}

bool operator==(const WebApplicationShortcutsMenuItemInfo& shortcut_info1,
                const WebApplicationShortcutsMenuItemInfo& shortcut_info2) {
  return std::tie(shortcut_info1.name, shortcut_info1.url, shortcut_info1.any,
                  shortcut_info1.maskable, shortcut_info1.monochrome) ==
         std::tie(shortcut_info2.name, shortcut_info2.url, shortcut_info2.any,
                  shortcut_info2.maskable, shortcut_info2.monochrome);
}

std::ostream& operator<<(std::ostream& out,
                         const WebApplicationShortcutsMenuItemInfo& info) {
  out << "name: " << info.name << std::endl;
  out << "  url: " << info.url << std::endl;
  out << "  icons:" << std::endl;
  out << "    any:" << std::endl;

  for (WebApplicationShortcutsMenuItemInfo::Icon icon : info.any) {
    out << "      url: " << icon.url << std::endl;
    out << "      square_size_px: " << icon.square_size_px << std::endl;
  }

  out << "    maskable:" << std::endl;
  for (WebApplicationShortcutsMenuItemInfo::Icon icon : info.maskable) {
    out << "      url: " << icon.url << std::endl;
    out << "      square_size_px: " << icon.square_size_px << std::endl;
  }

  out << "    monochrome:" << std::endl;
  for (WebApplicationShortcutsMenuItemInfo::Icon icon : info.monochrome) {
    out << "      url: " << icon.url << std::endl;
    out << "      square_size_px: " << icon.square_size_px << std::endl;
  }

  return out;
}
