// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/notification_badge_color_cache.h"

#include "base/no_destructor.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/image/image_skia.h"

namespace {

// Uses the icon image to calculate the light vibrant color to be used for
// the notification indicator.
base::Optional<SkColor> CalculateNotificationBadgeColor(gfx::ImageSkia image) {
  const SkBitmap* source = image.bitmap();
  if (!source || source->empty() || source->isNull())
    return base::nullopt;

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
    return base::nullopt;

  return best_swatches[0].color;
}

constexpr SkColor kDefaultIndicatorColor = SK_ColorWHITE;

}  // namespace

namespace ash {

NotificationBadgeColorCache& NotificationBadgeColorCache::GetInstance() {
  static base::NoDestructor<NotificationBadgeColorCache> color_cache;
  return *color_cache;
}

NotificationBadgeColorCache::NotificationBadgeColorCache() = default;

NotificationBadgeColorCache::~NotificationBadgeColorCache() = default;

SkColor NotificationBadgeColorCache::GetBadgeColorForApp(
    const std::string& app_id,
    gfx::ImageSkia icon) {
  AppIdBadgeColor::const_iterator it = app_id_badge_color_map_.find(app_id);
  if (it != app_id_badge_color_map_.end())
    return it->second;

  SkColor notification_color =
      CalculateNotificationBadgeColor(icon).value_or(kDefaultIndicatorColor);
  // TODO(crbug.com/1197249): Find a way to evict stale items in the
  // NotificationBadgeColorCache.
  app_id_badge_color_map_[app_id] = notification_color;
  return notification_color;
}

}  // namespace ash
