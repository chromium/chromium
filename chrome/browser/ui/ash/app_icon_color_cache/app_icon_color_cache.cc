// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/ash/app_icon_color_cache/app_icon_color_cache.h"

#include <array>
#include <memory>
#include <optional>
#include <set>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "services/preferences/public/cpp/dictionary_value_update.h"
#include "services/preferences/public/cpp/scoped_pref_update.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

BASE_FEATURE(kEnablePersistentAshIconColorCache,
             "EnablePersistentAshIconColorCache",
             base::FEATURE_ENABLED_BY_DEFAULT);

namespace {

// Constants -------------------------------------------------------------------

// An hsv color with a value less than this cutoff will be categorized as black.
constexpr float kBlackValueCutoff = 0.35f;

// When an hsv color has a saturation below 'kBlackWhiteSaturationCutoff' then
// if its value is below this cutoff it will be categorized as white and with a
// value above this cutoff is will be categorized as black.
constexpr float kBlackWhiteLowSaturatonValueCutoff = 0.9f;

// An hsv color with saturation below this cutoff will be categorized as either
// black or white.
constexpr float kBlackWhiteSaturationCutoff = 0.1f;

// A default return value for the GetLightVibrantColorForApp().
constexpr SkColor kDefaultLightVibrantColor = SK_ColorWHITE;

// On the 360 degree hue color spectrum, this value is used as a cutuff to
// indicate that any value equal to or higher than this is considered red.
constexpr float kRedHueCutoff = 315.0f;

// Utilities for the vibrant color prefs cache ---------------------------------

// Returns the vibrant color of the app icon specified by `key` from the
// prefs cache associated with `profile`. Returns `std::nullopt` if the queried
// color cannot be found.
std::optional<SkColor> GetLightVibrantColorForAppFromPrefsCache(
    Profile* profile,
    const std::string& key) {
  return profile->GetPrefs()
      ->GetDict(prefs::kAshAppIconLightVibrantColorCache)
      .FindInt(key);
}

// Sets the vibrant color of the app icon specified by `key` in the prefs
// cache associated with the `profile`. Overwrites the existing color in the
// cache if any.
void SetLightVibrantColorForAppInPrefsCache(Profile* profile,
                                            const std::string& key,
                                            SkColor color) {
  ::prefs::ScopedDictionaryPrefUpdate(profile->GetPrefs(),
                                      prefs::kAshAppIconLightVibrantColorCache)
      ->SetInteger(key, color);
}

// Utilities for the icon color prefs cache -----------------------------------
// NOTE: Prefs cache can only store primitive types. Therefore, we cache color
// groups and hues instead of `IconColor` instances.

void SetIntegerInPrefsDict(Profile* profile,
                           const std::string& dictionary,
                           const std::string& key,
                           int value) {
  ::prefs::ScopedDictionaryPrefUpdate(profile->GetPrefs(), dictionary)
      ->SetInteger(key, value);
}

void RemoveEntryFromColorPrefsCache(Profile* profile,
                                    const std::string& dictionary,
                                    const std::string& key) {
  ::prefs::ScopedDictionaryPrefUpdate(profile->GetPrefs(), dictionary)
      ->Remove(key);
}

std::optional<sync_pb::AppListSpecifics::ColorGroup>
GetColorGroupFromPrefsCache(Profile* profile, const std::string& app_id) {
  const auto result = profile->GetPrefs()
                          ->GetDict(prefs::kAshAppIconSortableColorGroupCache)
                          .FindInt(app_id);
  return result && sync_pb::AppListSpecifics::ColorGroup_IsValid(*result)
             ? std::optional<sync_pb::AppListSpecifics::ColorGroup>(
                   sync_pb::AppListSpecifics::ColorGroup(*result))
             : std::nullopt;
}

std::optional<int> GetHueFromPrefsCache(Profile* profile,
                                        const std::string& app_id) {
  const auto result = profile->GetPrefs()
                          ->GetDict(prefs::kAshAppIconSortableColorHueCache)
                          .FindInt(app_id);
  return result >= IconColor::kHueMin && result <= IconColor::kHueMax
             ? result
             : std::nullopt;
}

void StoreColorGroupToPrefsCache(
    Profile* profile,
    const std::string& app_id,
    sync_pb::AppListSpecifics::ColorGroup color_group) {
  SetIntegerInPrefsDict(profile, prefs::kAshAppIconSortableColorGroupCache,
                        app_id, color_group);
}

void StoreHueToPrefsCache(Profile* profile,
                          const std::string& app_id,
                          int hue) {
  SetIntegerInPrefsDict(profile, prefs::kAshAppIconSortableColorHueCache,
                        app_id, hue);
}

// Uses the icon image to calculate the light vibrant color.
std::optional<SkColor> CalculateLightVibrantColor(const gfx::ImageSkia& image) {
  TRACE_EVENT0("ui",
               "app_icon_color_cache::{anonynous}::CalculateLightVibrantColor");
  const SkBitmap* source = image.bitmap();
  if (!source || source->empty() || source->isNull())
    return std::nullopt;

  std::vector<color_utils::ColorProfile> color_profiles;
  color_profiles.emplace_back(color_utils::LumaRange::LIGHT,
                              color_utils::SaturationRange::VIBRANT);

  std::vector<color_utils::Swatch> best_swatches =
      color_utils::CalculateProminentColorsOfBitmap(
          *source, color_profiles, nullptr /* bitmap region */,
          color_utils::ColorSwatchFilter());

  // If the best swatch color is transparent, then
  // CalculateProminentColorsOfBitmap() failed to find a suitable color.
  if (best_swatches.empty() || best_swatches[0].color == SK_ColorTRANSPARENT)
    return std::nullopt;

  return best_swatches[0].color;
}

// Categorizes `color` into one color group.
sync_pb::AppListSpecifics::ColorGroup ColorToColorGroup(SkColor color) {
  TRACE_EVENT0("ui", "app_list::reorder::ColorToColorGroup");
  SkScalar hsv[3];
  SkColorToHSV(color, hsv);

  const float h = hsv[0];
  const float s = hsv[1];
  const float v = hsv[2];

  sync_pb::AppListSpecifics::ColorGroup group;

  // Assign the ColorGroup based on the hue of `color`. Each if statement checks
  // the value of the hue and groups the color based on it. These values are
  // approximations for grouping like colors together.
  if (h < 15) {
    group = sync_pb::AppListSpecifics::ColorGroup::
        AppListSpecifics_ColorGroup_COLOR_RED;
  } else if (h < 45) {
    group = sync_pb::AppListSpecifics::ColorGroup::
        AppListSpecifics_ColorGroup_COLOR_ORANGE;
  } else if (h < 75) {
    group = sync_pb::AppListSpecifics::ColorGroup::
        AppListSpecifics_ColorGroup_COLOR_YELLOW;
  } else if (h < 182) {
    group = sync_pb::AppListSpecifics::ColorGroup::
        AppListSpecifics_ColorGroup_COLOR_GREEN;
  } else if (h < 255) {
    group = sync_pb::AppListSpecifics::ColorGroup::
        AppListSpecifics_ColorGroup_COLOR_BLUE;
  } else if (h < kRedHueCutoff) {
    group = sync_pb::AppListSpecifics::ColorGroup::
        AppListSpecifics_ColorGroup_COLOR_MAGENTA;
  } else {
    group = sync_pb::AppListSpecifics::ColorGroup::
        AppListSpecifics_ColorGroup_COLOR_RED;
  }

  if (s < kBlackWhiteSaturationCutoff) {
    if (v < kBlackWhiteLowSaturatonValueCutoff) {
      group = sync_pb::AppListSpecifics::ColorGroup::
          AppListSpecifics_ColorGroup_COLOR_BLACK;
    } else {
      group = sync_pb::AppListSpecifics::ColorGroup::
          AppListSpecifics_ColorGroup_COLOR_WHITE;
    }
  } else if (v < kBlackValueCutoff) {
    group = sync_pb::AppListSpecifics::ColorGroup::
        AppListSpecifics_ColorGroup_COLOR_BLACK;
  }

  return group;
}

// Calculates the color group of the background of `source`.
// Samples color from the left, right, and top edge of the icon image and
// determines the color group for each. Returns the most common grouping from
// the samples. If all three sampled groups are different, then returns
// 'light_vibrant_group' which is the color group for the light vibrant color of
// the whole icon image.
sync_pb::AppListSpecifics::ColorGroup CalculateBackgroundColorGroup(
    const SkBitmap& source,
    sync_pb::AppListSpecifics::ColorGroup light_vibrant_group) {
  TRACE_EVENT0("ui", "app_list::reorder::CalculateBackgroundColorGroup");
  if (source.empty()) {
    return sync_pb::AppListSpecifics::ColorGroup::
        AppListSpecifics_ColorGroup_COLOR_WHITE;
  }

  DCHECK_EQ(kN32_SkColorType, source.info().colorType());

  const int width = source.width();
  const int height = source.height();

  sync_pb::AppListSpecifics::ColorGroup left_group = sync_pb::AppListSpecifics::
      ColorGroup::AppListSpecifics_ColorGroup_COLOR_BLACK;
  sync_pb::AppListSpecifics::ColorGroup right_group = sync_pb::
      AppListSpecifics::ColorGroup::AppListSpecifics_ColorGroup_COLOR_BLACK;

  // Find the color group for the first opaque pixel on the left edge of the
  // icon.
  const SkColor* current =
      reinterpret_cast<SkColor*>(source.getAddr32(0, height / 2));
  for (int x = 0; x < width; ++x, ++current) {
    if (SkColorGetA(*current) < SK_AlphaOPAQUE) {
      continue;
    }
    left_group = ColorToColorGroup(*current);
    break;
  }

  // Find the color group for the first opaque pixel on the right edge of the
  // icon.
  current = reinterpret_cast<SkColor*>(source.getAddr32(width - 1, height / 2));
  for (int x = width - 1; x >= 0; --x, --current) {
    if (SkColorGetA(*current) < SK_AlphaOPAQUE) {
      continue;
    }
    right_group = ColorToColorGroup(*current);
    break;
  }

  // If the left and right edge have the same color grouping, then return that
  // group as the calculated background color group.
  if (left_group == right_group) {
    return left_group;
  }

  // Find the color group for the first opaque pixel on the top edge of the
  // icon.
  sync_pb::AppListSpecifics::ColorGroup top_group = sync_pb::AppListSpecifics::
      ColorGroup::AppListSpecifics_ColorGroup_COLOR_BLACK;
  current = reinterpret_cast<SkColor*>(source.getAddr32(width / 2, 0));
  for (int y = 0; y < height; ++y, current += width) {
    if (SkColorGetA(*current) < SK_AlphaOPAQUE) {
      continue;
    }
    top_group = ColorToColorGroup(*current);
    break;
  }

  // If the top edge has a matching color group with the left or right group,
  // then return that group.
  if (top_group == right_group || top_group == left_group) {
    return top_group;
  }

  // When all three sampled color groups are different, then there is no
  // conclusive color group for the icon's background. Return the group
  // corresponding to the app icon's light vibrant color.
  return light_vibrant_group;
}

// Returns a `IconColor` which can be used to sort icons by their
// background color and light vibrant color.
IconColor CalculateIconColorForApp(const std::string& id,
                                   SkColor extracted_light_vibrant_color,
                                   const gfx::ImageSkia& image) {
  TRACE_EVENT0("ui", "app_icon_color_cache::CalculateIconColorForApp");

  const sync_pb::AppListSpecifics::ColorGroup light_vibrant_color_group =
      ColorToColorGroup(extracted_light_vibrant_color);

  // `hue` represents the hue of the extracted light vibrant color and can be
  // defined by the interval [-1, 360], where -1 (kHueMin) denotes that the hue
  // should come before all other hue values, and 360 (kHueMax) denotes that the
  // hue should come after all other hue values.
  int hue;

  if (light_vibrant_color_group ==
      sync_pb::AppListSpecifics::ColorGroup::
          AppListSpecifics_ColorGroup_COLOR_BLACK) {
    // If `light_vibrant_color_group` is black it should be ordered after all
    // other hues.
    hue = IconColor::kHueMax;
  } else if (light_vibrant_color_group ==
             sync_pb::AppListSpecifics::ColorGroup::
                 AppListSpecifics_ColorGroup_COLOR_WHITE) {
    // If 'light_vibrant_color_group' is white, then the hue should be ordered
    // before all other hues.
    hue = IconColor::kHueMin;

  } else {
    SkScalar hsv[3];
    SkColorToHSV(extracted_light_vibrant_color, hsv);
    hue = hsv[0];

    // If the hue is a red on the high end of the hsv color spectrum, then
    // subtract the maximum possible hue so that reds on the high end of the hsv
    // color spectrum are ordered next to reds on the low end of the hsv color
    // spectrum.
    if (hue >= kRedHueCutoff) {
      hue -= IconColor::kHueMax;
    }

    // Shift up the hue value so that the returned hue value always remains
    // within the interval [0, 360].
    hue += (IconColor::kHueMax - kRedHueCutoff);

    DCHECK_GE(hue, 0);
    DCHECK_LE(hue, IconColor::kHueMax);
  }

  return IconColor(
      CalculateBackgroundColorGroup(*image.bitmap(), light_vibrant_color_group),
      hue);
}

bool IsPersistentCacheEnabled() {
  return base::FeatureList::IsEnabled(kEnablePersistentAshIconColorCache);
}

std::map<Profile*, std::unique_ptr<AppIconColorCache>>&
GetInstanceStorageMap() {
  static base::NoDestructor<
      std::map<Profile*, std::unique_ptr<AppIconColorCache>>>
      color_cache;
  return *color_cache;
}

void DestroyInstance(Profile* profile) {
  GetInstanceStorageMap().erase(profile);
}

}  // namespace

AppIconColorCache& AppIconColorCache::GetInstance(Profile* profile) {
  auto& color_cache = GetInstanceStorageMap();
  auto it = color_cache.find(profile);
  if (it == color_cache.end()) {
    const auto [new_it, success] = color_cache.emplace(
        profile, std::make_unique<AppIconColorCache>(profile));
    CHECK(success);
    it = new_it;
  }
  return *(it->second);
}

// PartitionAlloc DanglingRawPtr detector will check on unused pointer so we
// only initialize it when needed.
AppIconColorCache::AppIconColorCache(Profile* profile)
    : profile_(IsPersistentCacheEnabled() ? profile : nullptr) {
  // AppIconColorCache is only valid for a real user so profile must be valid
  // if it is not used.
  DCHECK(profile);
  if (IsPersistentCacheEnabled()) {
    profile_observation_.Observe(profile);
  }
  // Clean up cached data.
  if (profile && !IsPersistentCacheEnabled()) {
    for (const auto* const dictionary :
         {prefs::kAshAppIconLightVibrantColorCache,
          prefs::kAshAppIconSortableColorGroupCache,
          prefs::kAshAppIconSortableColorHueCache}) {
      profile->GetPrefs()->ClearPref(dictionary);
    }
  }
}

AppIconColorCache::~AppIconColorCache() = default;

void AppIconColorCache::OnProfileWillBeDestroyed(Profile* profile) {
  profile_observation_.Reset();
  DestroyInstance(profile_);
}

SkColor AppIconColorCache::GetLightVibrantColorForApp(
    const std::string& app_id,
    const gfx::ImageSkia& icon) {
  if (IsPersistentCacheEnabled() && !profile_) {
    // This could happen when `profile_` is removed before the destruction of
    // `AppIconColorCache`.
    return kDefaultLightVibrantColor;
  }
  AppIdLightVibrantColor::const_iterator it =
      vibrant_colors_by_ids_.find(app_id);
  if (it != vibrant_colors_by_ids_.end()) {
    return it->second;
  }

  if (HasPrefsCache()) {
    if (const auto result =
            GetLightVibrantColorForAppFromPrefsCache(profile_, app_id)) {
      vibrant_colors_by_ids_[app_id] = *result;
      return *result;
    }
  }

  SkColor light_vibrant_color =
      CalculateLightVibrantColor(icon).value_or(kDefaultLightVibrantColor);
  // TODO(crbug.com/40176836): Find a way to evict stale items in the
  // AppIconColorCache.
  vibrant_colors_by_ids_[app_id] = light_vibrant_color;

  if (HasPrefsCache()) {
    SetLightVibrantColorForAppInPrefsCache(profile_, app_id,
                                           light_vibrant_color);
  }

  return light_vibrant_color;
}

IconColor AppIconColorCache::GetIconColorForApp(const std::string& app_id,
                                                const gfx::ImageSkia& image) {
  if (IsPersistentCacheEnabled()) {
    if (!profile_) {
      // This could happen when `profile_` is removed before the destruction of
      // `AppIconColorCache`.
      return IconColor();
    }
    if (const AppIdIconColor::const_iterator it =
            icon_colors_by_ids_.find(app_id);
        it != icon_colors_by_ids_.end()) {
      return it->second;
    }
  }

  std::optional<IconColor> result = GetIconColorForAppFromPrefsCache(app_id);
  if (!result) {
    result = CalculateIconColorForApp(
        app_id, GetLightVibrantColorForApp(app_id, image), image);
  }
  if (IsPersistentCacheEnabled()) {
    icon_colors_by_ids_[app_id] = *result;
  }
  MaybeStoreIconColorToPrefsCache(app_id, *result);
  return *result;
}

void AppIconColorCache::RemoveColorDataForApp(const std::string& app_id) {
  icon_colors_by_ids_.erase(app_id);
  vibrant_colors_by_ids_.erase(app_id);
  if (!HasPrefsCache()) {
    return;
  }
  RemoveEntryFromColorPrefsCache(
      profile_, prefs::kAshAppIconLightVibrantColorCache, app_id);
  RemoveEntryFromColorPrefsCache(
      profile_, prefs::kAshAppIconSortableColorGroupCache, app_id);
  RemoveEntryFromColorPrefsCache(
      profile_, prefs::kAshAppIconSortableColorHueCache, app_id);
}

std::optional<IconColor> AppIconColorCache::GetIconColorForAppFromPrefsCache(
    const std::string& app_id) {
  if (!HasPrefsCache()) {
    return std::nullopt;
  }

  const auto color_group = GetColorGroupFromPrefsCache(profile_, app_id);
  if (!color_group) {
    return std::nullopt;
  }

  const auto hue = GetHueFromPrefsCache(profile_, app_id);
  if (!hue) {
    return std::nullopt;
  }

  return IconColor(*color_group, *hue);
}

void AppIconColorCache::MaybeStoreIconColorToPrefsCache(
    const std::string& app_id,
    const IconColor& icon_color) {
  if (HasPrefsCache()) {
    StoreColorGroupToPrefsCache(profile_, app_id,
                                icon_color.background_color());
    StoreHueToPrefsCache(profile_, app_id, icon_color.hue());
  }
}

bool AppIconColorCache::HasPrefsCache() const {
  return IsPersistentCacheEnabled() && profile_;
}

}  // namespace ash
