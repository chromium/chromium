// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/app_icon_color_cache.h"

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/image/image_skia.h"

namespace {

// Uses the icon image to calculate the light vibrant color.
absl::optional<SkColor> CalculateLightVibrantColor(gfx::ImageSkia image) {
  const SkBitmap* source = image.bitmap();
  if (!source || source->empty() || source->isNull())
    return absl::nullopt;

  std::vector<color_utils::ColorProfile> color_profiles;
  color_profiles.push_back(color_utils::ColorProfile(
      color_utils::LumaRange::LIGHT, color_utils::SaturationRange::VIBRANT));

  std::vector<color_utils::Swatch> best_swatches =
      color_utils::CalculateProminentColorsOfBitmap(
          *source, color_profiles, nullptr /* bitmap region */,
          color_utils::ColorSwatchFilter());

  // If the best swatch color is transparent, then
  // CalculateProminentColorsOfBitmap() failed to find a suitable color.
  if (best_swatches.empty() || best_swatches[0].color == SK_ColorTRANSPARENT)
    return absl::nullopt;

  return best_swatches[0].color;
}

constexpr SkColor kDefaultLightVibrantColor = SK_ColorWHITE;

}  // namespace

namespace ash {

AppIconColorCache& AppIconColorCache::GetInstance() {
  static base::NoDestructor<AppIconColorCache> color_cache;
  return *color_cache;
}

AppIconColorCache::AppIconColorCache() = default;

AppIconColorCache::~AppIconColorCache() = default;

SkColor AppIconColorCache::GetLightVibrantColorForApp(const std::string& app_id,
                                                      gfx::ImageSkia icon) {
  AppIdLightVibrantColor::const_iterator it =
      app_id_light_vibrant_color_map_.find(app_id);
  if (it != app_id_light_vibrant_color_map_.end())
    return it->second;

  SkColor light_vibrant_color =
      CalculateLightVibrantColor(icon).value_or(kDefaultLightVibrantColor);
  // TODO(crbug.com/1197249): Find a way to evict stale items in the
  // AppIconColorCache.
  app_id_light_vibrant_color_map_[app_id] = light_vibrant_color;
  return light_vibrant_color;
}

}  // namespace ash
