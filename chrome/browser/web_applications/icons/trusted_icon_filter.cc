// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/icons/trusted_icon_filter.h"

#include <optional>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "chrome/browser/web_applications/web_app_icon_operations.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/services/app_service/public/cpp/icon_info.h"
#include "components/webapps/browser/installable/installable_evaluator.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace web_app {

namespace {

constexpr SquareSizePx kMaxIconSizeForMaskableIcons = 256;
constexpr SquareSizePx kIconSizeForSVGNoIntrinsicSize = 1024;

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
constexpr bool kPreferMaskableIcons = true;
#else
constexpr bool kPreferMaskableIcons = false;
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)

}  // namespace

std::optional<apps::IconInfo> GetTrustedIconsFromManifest(
    const std::vector<blink::Manifest::ImageResource>& icons) {
  // Keep track of largest icon per purpose based on all the entries in the
  // manifest.
  base::flat_map<IconPurpose, IconUrlWithSize> purpose_to_icon_data_map;
  // Keep track of any SVG icons of no size, if found.
  base::flat_map<IconPurpose, GURL> svg_icons_no_size;
  for (const auto& icon : icons) {
    CHECK(!icon.purpose.empty());

    for (IconPurpose purpose : icon.purpose) {
      if (base::Contains(icon.sizes, gfx::Size()) &&
          base::Contains(icon.src.spec(), ".svg")) {
        svg_icons_no_size[purpose] = icon.src;
      }

      for (const auto& size : icon.sizes) {
        // Exit early if sizes are empty, or if there are no square icons, or if
        // the size is more than the maximum size supported by the web
        // applications system.
        if (size.IsZero() || (size.width() != size.height()) ||
            (size.width() >
             webapps::InstallableEvaluator::kMaximumIconSizeInPx)) {
          continue;
        }

        auto it = purpose_to_icon_data_map.find(purpose);
        // Insert or assign if the purpose doesn't exist yet, or if the new icon
        // is larger than the one we already have for this purpose.
        if (it == purpose_to_icon_data_map.end() ||
            size.width() >= it->second.size.width()) {
          purpose_to_icon_data_map.insert_or_assign(
              purpose, IconUrlWithSize::Create(icon.src, size));
          continue;
        }
      }
    }
  }

  // Choose a maskable icon on MacOS and ChromeOS, otherwise choose an icon of
  // purpose `any`.
  auto maskable_it = purpose_to_icon_data_map.find(IconPurpose::MASKABLE);
  if (kPreferMaskableIcons && maskable_it != purpose_to_icon_data_map.end() &&
      maskable_it->second.size.width() >= kMaxIconSizeForMaskableIcons) {
    apps::IconInfo primary_icon_info;
    primary_icon_info.square_size_px = maskable_it->second.size.width();
    primary_icon_info.purpose = apps::IconInfo::Purpose::kMaskable;
    primary_icon_info.url = maskable_it->second.url;
    return primary_icon_info;
  }

  auto any_it = purpose_to_icon_data_map.find(IconPurpose::ANY);
  if (any_it != purpose_to_icon_data_map.end()) {
    apps::IconInfo primary_icon_info;
    primary_icon_info.square_size_px = any_it->second.size.width();
    primary_icon_info.purpose = apps::IconInfo::Purpose::kAny;
    primary_icon_info.url = any_it->second.url;
    return primary_icon_info;
  }

  // Fallback to using SVG icons if no existing primary_icon_info is found (AKA
  // if the manifest has icons of an empty size specified) with the same
  // behavior applied above.
  if (kPreferMaskableIcons &&
      base::Contains(svg_icons_no_size, IconPurpose::MASKABLE)) {
    apps::IconInfo primary_icon_info;
    primary_icon_info.square_size_px = kIconSizeForSVGNoIntrinsicSize;
    primary_icon_info.purpose = apps::IconInfo::Purpose::kMaskable;
    primary_icon_info.url = svg_icons_no_size[IconPurpose::MASKABLE];
    return primary_icon_info;
  }

  if (base::Contains(svg_icons_no_size, IconPurpose::ANY)) {
    apps::IconInfo primary_icon_info;
    primary_icon_info.square_size_px = kIconSizeForSVGNoIntrinsicSize;
    primary_icon_info.purpose = apps::IconInfo::Purpose::kAny;
    primary_icon_info.url = svg_icons_no_size[IconPurpose::ANY];
    return primary_icon_info;
  }

  // No primary icon was found after parsing, icons will be generated from the
  // title of the web app itself.
  return std::nullopt;
}

}  // namespace web_app
