// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_application_info.h"

#include "chrome/common/web_page_metadata.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

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

// WebApplicationInfo
WebApplicationInfo::WebApplicationInfo() = default;

WebApplicationInfo::WebApplicationInfo(const WebApplicationInfo& other) =
    default;

WebApplicationInfo::WebApplicationInfo(
    const chrome::mojom::WebPageMetadata& metadata)
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
    case chrome::mojom::WebPageMobileCapable::UNSPECIFIED:
      mobile_capable = MOBILE_CAPABLE_UNSPECIFIED;
      break;
    case chrome::mojom::WebPageMobileCapable::ENABLED:
      mobile_capable = MOBILE_CAPABLE;
      break;
    case chrome::mojom::WebPageMobileCapable::ENABLED_APPLE:
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
  out << "url: " << icon_info.url << " square_size_px: ";
  if (icon_info.square_size_px)
    out << *icon_info.square_size_px;
  else
    out << "none";
  out << " purpose: " << icon_info.purpose;
  return out;
}

bool operator==(const WebApplicationShortcutsMenuItemInfo::Icon& icon1,
                const WebApplicationShortcutsMenuItemInfo::Icon& icon2) {
  return std::tie(icon1.url, icon1.square_size_px) ==
         std::tie(icon2.url, icon2.square_size_px);
}

bool operator==(const WebApplicationShortcutsMenuItemInfo& shortcut_info1,
                const WebApplicationShortcutsMenuItemInfo& shortcut_info2) {
  return std::tie(shortcut_info1.name, shortcut_info1.url,
                  shortcut_info1.shortcut_icon_infos) ==
         std::tie(shortcut_info2.name, shortcut_info2.url,
                  shortcut_info2.shortcut_icon_infos);
}

std::ostream& operator<<(std::ostream& out,
                         const WebApplicationShortcutsMenuItemInfo& info) {
  out << "    name: " << info.name << std::endl
      << "    url: " << info.url << std::endl
      << "    shortcut_icon_infos:" << std::endl;
  for (WebApplicationShortcutsMenuItemInfo::Icon icon :
       info.shortcut_icon_infos) {
    out << "      icon url: " << icon.url << std::endl
        << "      icon square_size_px" << icon.square_size_px << std::endl;
  }
  return out;
}
