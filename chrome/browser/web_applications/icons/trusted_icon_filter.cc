// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/icons/trusted_icon_filter.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "chrome/browser/web_applications/web_app_icon_operations.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/services/app_service/public/cpp/icon_info.h"
#include "components/webapps/browser/installable/installable_evaluator.h"
#include "third_party/blink/public/common/manifest/manifest_icon_selector.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace web_app {

namespace {

constexpr SquareSizePx kMinIconSizeForMaskableIcons = 256;
constexpr SquareSizePx kIconSizeForSVGNoIntrinsicSize = 1024;

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
constexpr bool kPreferMaskableIcons = true;
#else
constexpr bool kPreferMaskableIcons = false;
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)

}  // namespace
std::optional<apps::IconInfo> GetTrustedIconsFromManifest(
    const std::vector<blink::Manifest::ImageResource>& icons) {
  blink::ManifestIconSelectorParams params;
  params.svg_handling =
      blink::ManifestIconSelectorParams::AnySizeSvgHandling::kAsFallback;
  params.ideal_icon_size_in_px =
      webapps::InstallableEvaluator::kMaximumIconSizeInPx;
  params.maximum_icon_size_in_px =
      webapps::InstallableEvaluator::kMaximumIconSizeInPx;
  params.limited_image_types_for_installable_icon = true;

  std::optional<blink::ManifestIconSelectorResult> result;
  if (kPreferMaskableIcons) {
    // Check for a maskable icon if one is preferred. Maskable icons have
    // a minimum size, unlike the general case.
    params.purpose = IconPurpose::MASKABLE;
    params.minimum_icon_size_in_px = kMinIconSizeForMaskableIcons;
    result = blink::ManifestIconSelector::FindBestMatchingIcon(icons, params);
  }

  if (!result) {
    params.purpose = IconPurpose::ANY;
    params.minimum_icon_size_in_px = 0;
    result = blink::ManifestIconSelector::FindBestMatchingIcon(icons, params);
  }

  if (!result) {
    return std::nullopt;
  }

  apps::IconInfo primary_icon_info;
  primary_icon_info.url = result->icon_url;
  primary_icon_info.purpose =
      ManifestPurposeToIconInfoPurpose(result->icon_purpose);
  if (result->icon_size.IsEmpty()) {
    primary_icon_info.square_size_px = kIconSizeForSVGNoIntrinsicSize;
  } else {
    primary_icon_info.square_size_px = result->icon_size.width();
  }

  return primary_icon_info;
}

}  // namespace web_app
